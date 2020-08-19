#include <iostream>
#include <unordered_map>
#include <memory>
#include <thread>
#include "SyncedFileServer.h"
#include "Jobs.h"
#include "ServerSocket.h"
#include "User.h"
#include <filesystem>

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

void deleteFile(const std::shared_ptr<SyncedFileServer>& sfp){
    if(synced_files.find(sfp->getPath())!=synced_files.end())
        synced_files.erase(sfp->getPath());
}

// richiedo il file solo se non è già presente o è diverso
void requestFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::shared_ptr<Socket>& sock){
    auto map_value = synced_files.find(sfp->getPath());
    if(map_value == synced_files.end() || map_value->second->getHash() != sfp->getHash()){
        // chiedo al client di mandarmi il file perchè non è presente
        sock->askFile();

        // ricevo il file e lo salvo in una cartella temporanea
        std::string temp_path = sock->getFile();

        // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
        if(SyncedFileServer::CalcSha256(temp_path) == sfp->getHash()){
            // todo: copio il file nel direttorio opportuno
            // aggiorno il valore della mappa
            synced_files[getUserPath(sock, sfp->getPath())] = sfp;
        }
        // altrimenti comunico il problema al client che gestirà l'errore
        else
            sock->fileError();
    }
}

void worker(){
    while (true){
        try {
            std::shared_ptr<Socket> sock = jobs.get();
            if(sock == nullptr){
                if(jobs.producer_is_ended()){
                    std::unique_lock lock_print(mutex_mex);
                    std::cout << "Exit thread" << std::endl;
                    lock_print.unlock();
                    return;
                }
            } else {
                std::string JSON = sock->readJSON();
                std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(JSON);
                switch (sfp->getFileStatus()){
                    case FileStatus::created:
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::modified:
                        requestFile(sfp, sock);
                        break;
                    case FileStatus::erased:
                        deleteFile(sfp);
                        break;
                    case FileStatus::not_valid:
                        // todo: fare qualcosa o ignorare o boh
                        break;
                }
                jobs.put(sock);
            }
        }
        catch (std::runtime_error& error) {

        }
    }
}

bool auth(const User& user){
    auto map_user = users.find(user.getUsername());
    if (map_user == users.end()){
        const std::string& username = user.getUsername();
        users[username] = user;
        return true;
    }
    return map_user->second.getPassword() == user.getPassword();
}

int main() {
    ServerSocket serverSocket(6015, backlog);
    std::vector<std::thread> threads;
    threads.reserve(max_thread);
    for(int i=0; i<max_thread; i++)
        threads.emplace_back(std::thread(worker));
    while (true) {
        try {
            struct sockaddr addr;
            socklen_t len;
            Socket s = serverSocket.accept(&addr, &len);
            // todo: implementare select
            // todo: questo rallenta l'aggiunta di connessioni, un solo thread si occcupa di gestire l'auth, con i giusti timeout penso sia accettabile
            // todo: gestire eccezioni
            std::string u(s.readJSON());
            User user(u);
            // todo: se avanza tempo si potrebbe usare diffie - hellman per scambiare chiavi e avere un canale sicuro
            std::cout << "username: " << user.getUsername() << std::endl;

            // se le credenziali sono valide lo aggiungo alla coda dei jobs,
            // altrimenti la connessione verrà chiusa poichè la socket sarà distrutta
            if(auth(user)){
                s.setUsername(user.getUsername());
                std::shared_ptr<Socket> ptr = std::make_shared<Socket>(std::move(s));
                jobs.put(ptr);
            }
        }
        catch (std::runtime_error &error) {
            // todo: gestire errore
            std::cout << error.what() << std::endl;
        }
    }
}
