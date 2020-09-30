//
// Created by stefano on 30/08/20.
//


#include "Server.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <shared_mutex>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <utility>
#include "Session.h"

namespace pt = boost::property_tree;

void Session::saveMap(){
    pt::ptree pt;
    std::filesystem::create_directory("synced_maps");
    for(auto const& [key, val] : (*user_map)){
//        std::cout << val->getPath() << std::endl;
        pt.push_back(std::make_pair(key, val->getPtree()));
    }
    pt::json_parser::write_json("synced_maps/" + username + ".json", pt);
}

boost::asio::ssl::stream<tcp::socket> &Session::getSocket(){
    return socket_;
}

Session::Session(tcp::socket socket, boost::asio::ssl::context &context):
socket_(std::move(socket), context), mode(ProtocolMode::UNDEFINED){
}

void Session::sendOKRespAndRestart(const std::shared_ptr<Session>& self) {
    if(!socket_.lowest_layer().is_open()){
        std::cout << "connessione chiusa" << std::endl;
        return;
    }
    std::string buffer = "OK\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
//                std::cout << "sendOKRespAndRestart: " <<  error.message()<< " " << error.value()  << std::endl;
                if(!socket_.lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    switch (this->mode) {
                        case ProtocolMode::UNDEFINED :
                            this->getMode(self);
                            break;
                        case ProtocolMode::BACK :
                            this->getInfoFile(self);
                            break;
                        case ProtocolMode::SYNC :
                            this->sendInfoFile(self);
                            break;
                    }

                }
                // toodo: gestire errore
//                        else this->sendKORespAndClose();
            });
}

void Session::sendKORespAndRestart(const std::shared_ptr<Session>& self) {
    if(!socket_.lowest_layer().is_open()){
        std::cout << "connessione chiusa" << std::endl;
        return;
    }
    std::string buffer = "KO\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
//                std::cout << "sendKORespAndRestart: " <<  error.message() << " " << error.value() << std::endl;
                if(!socket_.lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    this->clearBuffer();
                    this->getInfoFile(self);
                }
                // se l'invio del KO non va a buon fine chiudo la connessione
            });
}

void Session::sendKORespAndClose(std::shared_ptr<Session> self) {
    if(!socket_.lowest_layer().is_open()){
        std::cout << "connessione chiusa" << std::endl;
        return;
    }
    std::string buffer = "KO\\\n";
    std::cout << "sendKORespAndClose" << std::endl;
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << error.message() << std::endl;

                std::cout << "Chiudo la connessione per un errore" << std::endl;
            });
}

void Session::setUsername(std::string u) {
    std::unique_lock lock(Session::subscribers_mutex);
    std::cout << "Sessione aggiunta: " << u << std::endl;
    Session::subscribers_.insert(u);
    lock.unlock();
    this->username = std::move(u);
}

// genero un nome temporaneo per il file dato da tempo corrente + id thread
std::string Session::tempFilePath(){
    const int length = 50;
    std::filesystem::create_directory("temp/");
    auto randChar = []() -> char{
        const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ random() % max_index ];
    };
    unsigned seed = time(nullptr);
    srandom(seed);
    std::string str(length,0);
    std::generate_n( str.begin(), length, randChar );
//    std::cout << "temp/" << str << std::endl;
    return "temp/" + str;
}

void Session::moveFile(const std::shared_ptr<SyncedFileServer>& sfp, const std::string& tempPath){
    std::filesystem::path user_path("./users_files/");
    user_path += username;
    std::filesystem::path path(sfp->getPath());
    user_path += path;
    std::filesystem::create_directories(user_path.parent_path());
    std::filesystem::copy_file(tempPath, user_path, std::filesystem::copy_options::overwrite_existing);
    if(std::filesystem::is_directory(tempPath) || std::filesystem::is_regular_file(tempPath))
        std::filesystem::remove(tempPath);
//    std::cout << user_path.parent_path() << std::endl;
}

void Session::deleteFile(const std::shared_ptr<SyncedFileServer>& sfp){
    std::filesystem::path user_path("./users_files/");
    user_path += username;
    std::filesystem::path path(sfp->getPath());
    user_path += path;

    if(std::filesystem::is_directory(user_path) || std::filesystem::is_regular_file(user_path))
        std::filesystem::remove(user_path);
    std::cout << user_path << " erased" << std::endl;
}

void Session::deleteFileMap(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp){
    std::cout << "Elimino il file " << sfp->getPath() << std::endl;
    user_map->erase(sfp->getPath());
    deleteFile(sfp);
    saveMap();
    this->sendOKRespAndRestart(self);
    std::cout << "FILE OK" << std::endl;
}

void Session::getFileEnd(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp, const std::string& tempPath){
    // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
    if(SyncedFileServer::CalcSha256(tempPath) == sfp->getHash()){
//        std::cout << "sendo un ok" << std::endl;

//        std::cout << "FILE OK (" << tempPath << " - " << sfp->getHash() << ')' << std::endl;
        moveFile(sfp, tempPath);
        // aggiorno il valore della mappa
        (*user_map)[sfp->getPath()] = sfp;
        saveMap();
        this->sendOKRespAndRestart(self);
    }
        // altrimenti comunico il problema al client che gestirà l'errore
    else {
//        std::cout << "sendo un ko" << std::endl;

        std::cout << "File non valido (" << tempPath<< "): hash errato(" << SyncedFileServer::CalcSha256(tempPath) << ')' << std::endl;
        std::filesystem::remove(tempPath);
        this->sendKORespAndRestart(self);
    }
};

void Session::getFileR(
        const std::shared_ptr<Session>& self,
        const std::shared_ptr<SyncedFileServer>& sfp,
        const std::shared_ptr<std::ofstream>& file_ptr,
        const std::string& filePath,
        ssize_t sizeRead){
    // il file che ricevo è già aperto, ma verifico che lo sia
    if(!file_ptr->is_open()){
        std::cout << "File not opened: " << filePath << std::endl;
        this->sendKORespAndRestart(self);
    }
    if(sizeRead>=sfp->getFileSize()){
        std::cout << "Ho terminato il trasferimento del file" << std::endl;
        file_ptr->close();
        this->getFileEnd(self, sfp, filePath);
        return;
    }
//    std::cout << "Size read: " << sizeRead << std::endl;
    const ssize_t size_left = sfp->getFileSize()-sizeRead;
//    std::cout << "size_left: " <<  size_left << std::endl;

    // scelgo la dimensione massima del buffer
    const ssize_t buff_size = size_left<N ? size_left: N;
//    std::cout << "buff_size: " <<  buff_size << std::endl;

    boost::asio::async_read(
            socket_,
            boost::asio::buffer(data_, buff_size),
            [this, self, sfp, sizeRead, file_ptr, filePath](
                    const boost::system::error_code& error, // Result of operation.
                    std::size_t bytes_transferred           // Number of bytes copied into the buffer
            ){

//                std::cout << "getFileR: " <<  error.message() << " " << error.value() << std::endl;
//                std::cout << "sock: " <<  socket_.lowest_layer().is_open() << std::endl;

                if(!file_ptr->is_open()){
                    std::cout << "File not opened: " << filePath << std::endl;
                }
                if(!error){
//                    std::cout << "bytes_transferred: " <<  bytes_transferred << std::endl;
                    // se non si sono verificati errori scrivo il file
                    data_[bytes_transferred] = '\0';
                    file_ptr->write(data_, bytes_transferred);
                    this->getFileR(self, sfp, file_ptr, filePath, sizeRead+bytes_transferred);
                } else {
                    data_[bytes_transferred] = '\0';
                    file_ptr->close();
                    std::cout << "Errore getFileR" << std::endl;

                    std::filesystem::remove(filePath);
                    // se si verifica un errore di rete chiudo tutto
//                    this->sendKORespAndRestart(self);
                }
            });

}


void Session::getFile(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp){
    // creo un nome file provvisorio univoco tra tutti i thread
    std::string filePath(tempFilePath());

    // apro il file
    auto file_ptr = std::make_shared<std::ofstream>(filePath, std::ios::binary);

    // avvio la callback ricorsiva
    this->getFileR(self, sfp, file_ptr, filePath, 0);
}

// richiedo il file solo se non è già presente o è diverso
void Session::sendNORespAndGetFile(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp) {
    // ogni thread lavora solo in lettura sulla mappa totale e lettura e scrittura sulle figlie
    // => la mappa figlia è usata solo da un thread per volta

    auto map_value = user_map->find(sfp->getPath());
    if(map_value == user_map->end() || map_value->second->getHash() != sfp->getHash()){
        // chiedo al client di mandarmi il file perchè non è presente
        std::cout << "Richiedo il file al client" << std::endl;

        std::string buffer = "NO\\\n";
        boost::asio::async_write(
                socket_,
                boost::asio::buffer(buffer, buffer.size()),
                [this, sfp, self](const boost::system::error_code& error, std::size_t bytes_transferred){
//                    std::cout << "sendNORespAndGetFile: " <<  error.message() << " " << error.value() << std::endl;

                    if(!error){
//                        std::cout << "FILE NO" << std::endl;
                        this->getFile(self, sfp);
                    } else this->sendKORespAndClose(self);
                });
    }else {
        std::cout << "Il file è già presente" << std::endl;

        // il file è già presente quindi devo solo mandare un OK
        this->sendOKRespAndRestart(self);
    }
}

void Session::clearBuffer(){
    boost::asio::socket_base::bytes_readable command(true);
    socket_.lowest_layer().io_control(command);
    std::size_t bytes_readable = command.get();
    std::cout << "Il buffer contiene " << bytes_readable << "bytes" << std::endl;
    if(bytes_readable){
        boost::system::error_code ec;
        // todo: non funziona la read, il buffer non si svuota
        ssize_t bytes_read = boost::asio::read(socket_,boost::asio::buffer(data_, bytes_readable),ec);
        // ignoro gli errori
        std::cout << ec.message() << std::endl;
        std::cout << "Ho svuotato il buffer " << bytes_read << std::endl;
    }
}

void Session::getInfoFile(const std::shared_ptr<Session>& self) {
    if(!socket_.lowest_layer().is_open()){
        std::cout << "connessione chiusa" << std::endl;
        return;
    }
    // attendo un json con le informazioni sul file
    std::string data = boost::asio::buffer_cast<const char*>(this->buf.data());

    boost::asio::async_read_until(
            socket_,
            this->buf,
            "\\\n",
            [this, self](const boost::system::error_code& error, std::size_t bytes_transferred){
//                std::cout << "getInfoFile: " <<  error.message() << " " << error.value() << std::endl;
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(this->buf.data());
                        buf.consume(bytes_transferred);
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string json = data.substr(0, bytes_transferred-2);
//                        std::cout << json << "size: " << bytes_transferred << std::endl;
                        std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(json);
                        switch (sfp->getFileStatus()) {
                            case FileStatus::created:
                                std::cout << "Backup(" << this->username << "): " << sfp->getPath() << std::endl;
                                sendNORespAndGetFile(self, sfp);
                                break;
                            case FileStatus::modified:
                                std::cout << "Backup(" << this->username << ", update): " << sfp->getPath() << std::endl;
                                sendNORespAndGetFile(self, sfp);
                                break;
                            case FileStatus::erased:
                                std::cout << "Delete(" << this->username << "): " << sfp->getPath() << std::endl;
                                deleteFileMap(self, sfp);
                                break;
                            case FileStatus::not_valid:
                                std::cout << "Not valid: " << sfp->getPath() << std::endl;
                                this->sendKORespAndRestart(self);
                                break;
                        }
                    } catch (std::runtime_error &exception) {
                        // se si verifica un errore potrebbe essere dovuto dal filesystem o da un errore sui dati
                        // loggo l'errore e riavvio il lavoro dopo avere mandato un KO al client
                        std::cout << exception.what() << std::endl;
//                        this->clearBuffer();
                        this->sendKORespAndRestart(self);
                    }
                }
//                else this->sendKORespAndRestart(self);
            });
}

void Session::setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap){
    this->user_map = std::move(userMap);
}

Session::~Session() {
    std::cout << "Sessione distrutta " << this->username << std::endl;
    std::unique_lock lock(Session::subscribers_mutex);
    Session::subscribers_.erase(this->username);
}

std::shared_ptr<Session> Session::create(tcp::socket socket, boost::asio::ssl::context &context) {
    return std::make_shared<Session>(std::move(socket), context);
}

bool Session::isLogged(const std::string& username) {
    std::shared_lock lock(Session::subscribers_mutex);
    auto it = Session::subscribers_.find(username);
    std::string l = it!=Session::subscribers_.end()? "true": "false";
    std::cout << "Is " << username << " already logged? " <<  l << std::endl;
    return it!=Session::subscribers_.end();
}

void Session::getMode(const std::shared_ptr<Session>& session) {
    boost::asio::async_read_until(
            this->getSocket(),
            this->buf,
            "\\\n",
            [this, session](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the client
            ){
//                std::cout << "getMode: " <<  error.message() << std::endl;
                if(!session->getSocket().lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(session->buf.data());
                        session->buf.consume(bytes_transferred);
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string mode_r = data.substr(0, bytes_transferred-2);
//                        std::cout << data << "size: " << bytes_transferred << std::endl;
                        if(mode_r == "MODE_BACK"){
                            this->mode = ProtocolMode::BACK;
                            this->sendOKRespAndRestart(session);
                        }
                        else if(mode_r == "MODE_SYNC") {
                            this->mode = ProtocolMode::SYNC;
                            this->sendOKRespAndRestart(session);
                        }
                        else{
                            this->sendKORespAndClose(session);
                        }
                    } catch(std::runtime_error& e) {
                        std::cout << e.what() << std::endl;
                        // questa eccezione è generata da un'errore sul parsing dei dati di auth o del file system,
                        // quindi chiudo la connessione dopo avere notificato il client
                        this->sendKORespAndClose(session);
                    }
                }
            });
}

void Session::sendInfoFile(const std::shared_ptr<Session>& session){
    if(session->user_map->empty()){
        // se non ha file da mandare chiudere la connessione
        sendEndRestoreAndClose(session);
        //sendKORespAndClose(session);
    }
    this->next_file = session->user_map->begin();
    this->retry_count = 0;
    this->sendJSONFileR(session);
}

void Session::sendJSONFileR(const std::shared_ptr<Session>& session){
    // se il next_file è arrivato alla fine della mappa termina;
    auto& file_iterator = session->next_file;
    if(file_iterator == session->user_map->end()) {
        this->sendEndRestoreAndClose(session);
        return;
    }

    // se il synced file non è un regular file, passa avanti;
    if(!file_iterator->second->isFile()){
        this->retry_count = 0;
        this->next_file++;
        sendJSONFileR(session);
    }
    std::cout << "SYNC(" << this->username << "): " << this->next_file->second->getPath() << std::endl;
    std::string buffer = file_iterator->second->getJSON() + "\\\n";
    boost::asio::async_write(socket_, boost::asio::buffer(buffer),
                             [this, session](const boost::system::error_code& error,
                                 std::size_t size)
     {
//         std::cout << "sendJSONFileR: " <<  error.message() << " " << error.value()  << std::endl;
         if(!socket_.lowest_layer().is_open()){
             std::cout << "connessione chiusa" << std::endl;
             return;
         }
        if(!error){//&& size == buffer.length()){
            this->getResp(session);
        }
     });

}

void Session::getResp(const std::shared_ptr<Session>& session) {
    boost::asio::async_read_until(
            this->getSocket(),
            this->buf,
            "\\\n",
            [this, session](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the client
            ){
//                std::cout << "getResp: " <<  error.message() << std::endl;
                if(!session->getSocket().lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(session->buf.data());
                        session->buf.consume(bytes_transferred);
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string resp = data.substr(0, bytes_transferred-2);
//                        std::cout << data << "size: " << bytes_transferred << std::endl;

                        //Il client ha ricevuto il JSON, e non ha bisogno del file corrente, quindi passa al prossimo
                        //Oppure il file ha ricevuto correttamente il file binario e quindi passa al prossimo
                        if(resp == "OK"){
                            std::cout << "INVIO COMPLETATO: " << this->next_file->second->getPath() << std::endl;
                            this->retry_count = 0;
                            this->next_file++;
                            this->sendJSONFileR(session);
                        }

                        //Il client non possiede il file e quindi si provvede all'invio del binario
                        // oppure ci sono stati errori di trasmissione e quindi si riprova ad inviarlo.
                        else if(resp == "NO") {
                            this->sendBinaryFile(session);
                        } else if (resp == "KO"){
                            if (retry_count<5){
                                this->retry_count++;
                                this->sendJSONFileR(session);
                            } else {
                                this->sendKORespAndClose(session);
                            }
                        }
                        else{
                            this->sendKORespAndClose(session);
                        }
                    } catch(std::runtime_error& e) {
                        std::cout << e.what() << std::endl;
                        // questa eccezione è generata da un'errore sul parsing dei dati di auth o del file system,
                        // quindi chiudo la connessione dopo avere notificato il client
                        this->sendKORespAndClose(session);
                    }
                }
            });
}

void Session::sendBinaryFile(const std::shared_ptr<Session>& session){
    //a questo punto dovrebbe essere sicuro che il synced file sia un regular file in quanto testato su sendJSON
    auto& file = session->next_file->second;
    std::filesystem::path file_path("./users_files/");
    file_path += username;
    file_path += std::filesystem::path(file->getPath());
    sendBinaryFileR(session, std::make_shared<std::ifstream>(file_path, std::ios::binary));
}

void Session::sendBinaryFileR(const std::shared_ptr<Session>& session, const std::shared_ptr<std::ifstream>& file_to_send){
    char buffer[N];
    file_to_send->read(buffer, sizeof(char)*N);
    size_t byte_to_write = file_to_send->gcount();

    //Se ha finito di leggere il file, aspetta risposta:
    if(byte_to_write <= 0) {
        file_to_send->close();
        this->getResp(session);
        return;
    }
//    std::cout << byte_to_write << std::endl;

    boost::asio::async_write(socket_, boost::asio::buffer(buffer, byte_to_write),
                             [&, session, file_to_send](const boost::system::error_code& error,
                                 std::size_t size)
                             {
//                                 std::cout << "sendBinaryFileR: " <<  error.message() << " " << error.value()  << std::endl;
                                 if(!socket_.lowest_layer().is_open()){
                                     std::cout << "connessione chiusa" << std::endl;
                                     return;
                                 }
                                 if(!error){

                                     //Continua ad inviare fino ad esaurimento byte
                                     this->sendBinaryFileR(session, file_to_send);

                                 }
                             });

}

void Session::sendEndRestoreAndClose(const std::shared_ptr<Session>& session) {
    std::string buffer = "END\\\n";
    std::cout << "sendEndRestoreAndClose" << std::endl;
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.length()),
            [&, session](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << error.message() << std::endl;

                std::cout << "Chiudo connessione perchè terminato il restore dei file" << std::endl;
            });
}

std::set<std::string> Session::subscribers_;
std::shared_mutex Session::subscribers_mutex;


