#include <iostream>
#include <unordered_map>
#include <memory>
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

static const int max_thread = 10;
static const int backlog = 10;

std::mutex mutex;
std::mutex mutex_map;
std::mutex mutex_mex;

std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>> synced_files;
std::unordered_map<std::string, User> users;

Jobs jobs(max_thread * 10);

std::string getUserPath(const std::shared_ptr<Socket>& socket, std::string path){
    std::filesystem::path p(socket->getUsername());
    p.append(path);
    std::cout << path << std::endl;
    return p;
}

void deleteFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::shared_ptr<Socket>& sock){
    if(synced_files.find(sfp->getPath())!=synced_files.end()){
        synced_files.erase(getUserPath(sock, sfp->getPath()));
    }
    sock->sendOKResp();
    std::cout << "FILE OK" << std::endl;
}

// richiedo il file solo se non è già presente o è diverso
void requestFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::shared_ptr<Socket>& sock){
    auto map_value = synced_files.find(getUserPath(sock, sfp->getPath()));
    if(map_value == synced_files.end() || map_value->second->getHash() != sfp->getHash()){
        // chiedo al client di mandarmi il file perchè non è presente
        sock->sendNOResp();
        std::cout << "FILE NO" << std::endl;

        // ricevo il file e lo salvo in una cartella temporanea
        std::string temp_path = sock->getFile(sfp->getFileSize());

        // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
        if(SyncedFileServer::CalcSha256(temp_path) == sfp->getHash()){
            std::cout << "FILE OK" << std::endl;
            // todo: copio il file nel direttorio opportuno
            // aggiorno il valore della mappa
            synced_files[getUserPath(sock, sfp->getPath())] = sfp;
            sock->sendOKResp();
        }
        // altrimenti comunico il problema al client che gestirà l'errore
        else {
            std::cout << "FILE K0(" << temp_path<< "): hash errato(" << SyncedFileServer::CalcSha256(temp_path) << ')' << std::endl;
            sock->sendKOResp();
        }
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
                sock->clearReadBuffer();
                std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(JSON);
                switch (sfp->getFileStatus()){
                    case FileStatus::created:
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::modified:
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::erased:
                        deleteFile(sfp, sock);
                        break;
                    case FileStatus::not_valid:
                        sock->sendKOResp();
                        break;
                }
                jobs.put(sock);
            } catch (socketException &exception) {
                // scrivo l'errore e chiudo la socket
                std::cout << exception.what() << std::endl;
            } catch (dataException &exception) {
                // loggo l'errore e riaggiungo la connessione alla lista dei jobs dopo avere mandato un KO al client
                std::cout << exception.what() << std::endl;
                sock->sendKOResp();
                sock->clearReadBuffer();
                jobs.put(sock);
            } catch (boost::wrapexcept<boost::property_tree::ptree_bad_path> &e1 ) {
                std::cout << e1.what() << std::endl;
                sock->sendKOResp();
                sock->clearReadBuffer();
                jobs.put(sock);
            } catch (boost::wrapexcept<boost::property_tree::json_parser::json_parser_error> &e2) {
                std::cout << e2.what() << std::endl;
                sock->sendKOResp();
                sock->clearReadBuffer();
                jobs.put(sock);
            }
        }
    }
}


//void test_recv(){
//    ServerSocket serverSocket(6019, backlog);
//    struct sockaddr addr;
//    socklen_t len;
//    Socket s = serverSocket.accept(&addr, &len);
//    std::string path = s.getFile(16);
//    std::cout << "PATH: " << path << std::endl;
//    std::cout << "HASH: " << SyncedFileServer::CalcSha256(path) << std::endl;
//    s.sendOKResp();
//}

void loadMaps(){

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

void saveMap(std::string username){

}

bool auth(const User& user){
    auto map_user = users.find(user.getUsername());
    if (map_user == users.end()){
        // l'username deve essere lungo almeno 3 caratteri
        if(user.getUsername().length()<3)
            return false;
        // la password non deve contenere spazi
        for(char i : user.getPassword())
            if(i==' ')
                return false;
        const std::string& username = user.getUsername();
        users[username] = user;
        saveUsername(user);
        return true;
    }
    return map_user->second.getPassword() == user.getPassword();
}

int main() {
    try {
        loadUsers();
    } catch (filesystemException &exception) {
        // todo: se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
        std::cout << exception.what() << std::endl;
    }
    try {
        loadMaps();
    } catch (filesystemException &exception) {
        // todo: se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
        std::cout << exception.what() << std::endl;
    }
    try {
        ServerSocket serverSocket(6020, backlog);
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
