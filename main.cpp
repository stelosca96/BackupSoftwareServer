#include <iostream>
#include <unordered_map>
#include <thread>
#include "SyncedFileServer.h"
#include "Jobs.h"
#include "ServerSocket.h"
#include "User.h"
#include "exceptions/socketException.h"
#include "exceptions/dataException.h"
#include "exceptions/filesystemException.h"
#include <filesystem>
#include <fstream>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <shared_mutex>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

static const int max_thread = 10;
static const int backlog = 10;

std::mutex mutex;
std::shared_mutex mutex_map;
std::mutex mutex_mex;

std::unordered_map<std::string, std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>>> synced_files;
std::unordered_map<std::string, User> users;

Jobs jobs(max_thread * 10);

void moveFile(const std::string& username, const std::shared_ptr<SyncedFileServer>& sfp, const std::string& tempPath){
    std::filesystem::path user_path("./users_files/");
    user_path += username;
    std::filesystem::path path(sfp->getPath());
    user_path += path;
    std::filesystem::create_directories(user_path.parent_path());
    std::filesystem::copy_file(tempPath, user_path);
    std::filesystem::remove(tempPath);
    std::cout << user_path.parent_path() << std::endl;
}

void create_empty_map(const std::string& username){
    std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>> user_map;
    std::unique_lock lock(mutex_map);
    synced_files[username] = std::make_shared<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>>(std::move(user_map));
}

void saveMap(const std::string& username){
    // todo: gestire eccezioni
    pt::ptree pt;
    std::shared_lock map_lock(mutex_map);
    auto user_map = synced_files.find(username)->second;
    map_lock.unlock();
    for(auto const& [key, val] : (*user_map)){
//        std::cout << val->getPath() << std::endl;
        pt.push_back(std::make_pair(key, val->getPtree()));
    }
    pt::json_parser::write_json("synced_maps/" + username + ".json", pt);
}

void loadMap(const std::string& username){
    std::shared_lock lock(mutex_map);
    auto user_map = synced_files.find(username)->second;
    lock.unlock();
    // todo: gestire eccezioni
    pt::ptree root;
    pt::read_json("synced_maps/" + username + ".json", root);
//    std::cout << "File loaded for " << username << ':' << std::endl;
    for(const auto& p: root){
        std::stringstream ss;
        pt::json_parser::write_json(ss, p.second);
        (*user_map)[p.first] = std::make_shared<SyncedFileServer>(ss.str());
//        std::cout << p.first << std::endl;
    }
}

void loadMaps(){
    std::filesystem::create_directory("synced_maps");
    for(auto const& [key, val] : users){
        create_empty_map(key);
        try {
            loadMap(key);
        } catch (std::runtime_error &error) {
            std::cout << error.what() << std::endl;
        }
    }
}

void deleteFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::shared_ptr<Socket>& sock){
    if(synced_files.find(sfp->getPath())!=synced_files.end()){
        std::shared_lock map_lock(mutex_map);
        auto user_map = synced_files.find(sock->getUsername())->second;
        map_lock.unlock();
        user_map->erase(sfp->getPath());
    }
    sock->sendOKResp();
    std::cout << "FILE OK" << std::endl;
}

// richiedo il file solo se non è già presente o è diverso
void requestFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::shared_ptr<Socket>& sock){
    // ogni thread lavora solo in lettura sulla mappa totale e lettura e scrittura sulle figlie => la mappa figlia è usata solo da un thread per volta
    // todo: eliminare tutti i file in caso di eccezione
    std::cout << "Attendo lock" << std::endl;
    std::shared_lock map_lock(mutex_map);
    std::cout << "lock preso" << std::endl;
    auto user_map = synced_files.find(sock->getUsername())->second;
    map_lock.unlock();
    std::cout << "lock rilasciato" << std::endl;
    auto map_value = user_map->find(sfp->getPath());
    if(map_value == user_map->end() || map_value->second->getHash() != sfp->getHash()){
        // chiedo al client di mandarmi il file perchè non è presente
        std::cout << "sendo un no" << std::endl;

        sock->sendNOResp();
        std::cout << "FILE NO" << std::endl;

        // ricevo il file e lo salvo in una cartella temporanea
        std::string temp_path = sock->getFile(sfp->getFileSize());

        // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
        if(SyncedFileServer::CalcSha256(temp_path) == sfp->getHash()){
            std::cout << "sendo un ok" << std::endl;

            std::cout << "FILE OK (" << temp_path << " - " << sfp->getHash() << ')' << std::endl;
            // todo: gestire eccezioni della moveFile => ko in caso di errore
            moveFile(sock->getUsername(), sfp, temp_path);
            // aggiorno il valore della mappa
            (*user_map)[sfp->getPath()] = sfp;
            sock->sendOKResp();
        }
        // altrimenti comunico il problema al client che gestirà l'errore
        else {
            std::cout << "sendo un ko" << std::endl;

            std::cout << "FILE K0(" << temp_path<< "): hash errato(" << SyncedFileServer::CalcSha256(temp_path) << ')' << std::endl;
            // todo: elimino il file
            std::filesystem::remove(temp_path);
            sock->sendKOResp();
        }
    }else {
        // il file è già presente quindi devo solo mandare un OK
        sock->sendOKResp();
    }
}

void worker(){
    while (true){
        // la coda di jobs è thread safe
        std::shared_ptr<Socket> sock = jobs.get();
        if(sock == nullptr){
            if(jobs.producer_is_ended()){
                std::unique_lock lock_print(mutex_mex);
                std::cout << "Exit thread" << std::endl;
                lock_print.unlock();
                return;
            }
        } else {
            try {
                std::string JSON = sock->getJSON();
                std::cout << JSON << std::endl;
                std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(JSON);
                switch (sfp->getFileStatus()){
                    case FileStatus::created:
                        std::cout << "created" << std::endl;
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::modified:
                        std::cout << "modified" << std::endl;
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::erased:
                        std::cout << "erased" << std::endl;
                        deleteFile(sfp, sock);
                        break;
                    case FileStatus::not_valid:
                        std::cout << "not valid" << std::endl;
                        sock->sendKOResp();
                        break;
                }
                saveMap(sock->getUsername());
                jobs.put(sock);
            } catch (socketException &exception) {
                // scrivo l'errore e chiudo la socket
                std::cout << exception.what() << std::endl;
            } catch (std::runtime_error &exception) {
                // loggo l'errore e riaggiungo la connessione alla lista dei jobs dopo avere mandato un KO al client
                std::cout << exception.what() << std::endl;
                sock->sendKOResp();
                sock->clearReadBuffer();
                jobs.put(sock);
            }
//            catch (boost::wrapexcept<boost::property_tree::ptree_bad_path> &e1 ) {
//                std::cout << e1.what() << std::endl;
//                sock->sendKOResp();
//                sock->clearReadBuffer();
//                jobs.put(sock);
//            } catch (boost::wrapexcept<boost::property_tree::json_parser::json_parser_error> &e2) {
//                std::cout << e2.what() << std::endl;
//                sock->sendKOResp();
//                sock->clearReadBuffer();
//                jobs.put(sock);
//            }
        }
    }
}

void loadUsers(){
    std::cout << "Load users" << std::endl;
    std::ifstream infile("users_list.conf");
    std::string username, password;
    while (infile >> username >> password){
        std::cout << "U: " << username << " P: " << password << std::endl;
        users[username] = User(username, password);
    }
}

void saveUsername(const User& user){
    // il salvataggio degli utenti è gestito solo dal thread principale quindi non ho bisogno di sincronizzazione
    std::cout << "Save user" << std::endl;
    // todo: gestire eccezione
    std::ofstream outfile;
    outfile.open("users_list.conf", std::ios_base::app); // append instead of overwrite
    outfile << user.getUsername() << ' ' << user.getPassword() << std::endl;
    outfile.close();
}

bool auth(const User& user){
    auto map_user = users.find(user.getUsername());
    if (map_user == users.end()){
        // l'username deve essere lungo almeno 3 caratteri
        if(user.getUsername().length()<3)
            return false;
        // l'username non deve contenere spazi o / o \ e valutare altri caratteri speciali
        // todo: non permettere nomi che contengano caratteri speciali non possibili su windows
        for(char i : user.getUsername())
            if(i==' ' || i=='/' || i=='\\')
                return false;
        // la password non deve contenere spazi
        for(char i : user.getPassword())
            if(i==' ')
                return false;
        // todo: controllare che non esista già una connessione per quell'utente
        const std::string& username = user.getUsername();
        users[username] = user;
        create_empty_map(username);
        saveUsername(user);
        return true;
    }
    return map_user->second.getPassword() == user.getPassword();
}

int main() {
    try {
        loadUsers();
        try {
            loadMaps();
        } catch (filesystemException &exception) {
            // todo: se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
            std::cout << exception.what() << std::endl;
        }
    } catch (filesystemException &exception) {
        // todo: se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
        std::cout << exception.what() << std::endl;
    }

    try {
        ServerSocket serverSocket(6031, backlog);
        std::vector<std::thread> threads;
        threads.reserve(max_thread);
        for(int i=0; i<max_thread; i++)
            threads.emplace_back(std::thread(worker));
        while (true) {
            struct sockaddr addr;
            socklen_t len;
            Socket s = serverSocket.accept(&addr, &len);
            // questo rallenta l'aggiunta di connessioni,
            // un solo thread si occcupa di gestire l'auth, ma con i giusti timeout penso sia accettabile
            try {
                std::string json = s.getJSON();
                User user(json);
                // todo: se avanza tempo si potrebbe usare diffie - hellman per scambiare chiavi e avere un canale sicuro
                std::cout << "username: " << user.getUsername() << std::endl;

                // se le credenziali sono valide lo aggiungo alla coda dei jobs,
                // altrimenti la connessione verrà chiusa poichè la socket sarà distrutta
                if(auth(user)){
                    s.setUsername(user.getUsername());
                    s.sendOKResp();
                    std::shared_ptr<Socket> ptr = std::make_shared<Socket>(std::move(s));
                    jobs.put(ptr);
                } else s.sendKOResp();
            } catch (socketException &exception) {
                // se si verifica una socket exception non la aggiungo alla lista dei jobs, così che venga chiusa
                std::cout << exception.what() << std::endl;
            }
        }
    }
    catch (std::runtime_error &error) {
        // se si verifica un errore con la gestione della server socket chiudo il programma
        std::cout << error.what() << std::endl;
    }
}
