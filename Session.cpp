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
    // todo: gestire eccezioni
    pt::ptree pt;
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
        socket_(std::move(socket), context){
}

void Session::sendOKRespAndRestart(std::shared_ptr<Session> self) {
    std::string buffer = "OK\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "sendOKRespAndRestart: " <<  error.message()<< " " << error.value()  << std::endl;
                if(!socket_.lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    this->getInfoFile(self);
                }
                // toodo: gestire errore
//                        else this->sendKORespAndClose();
            });
}

void Session::sendKORespAndRestart(std::shared_ptr<Session> self) {
    std::string buffer = "KO\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "sendOKRespAndRestart: " <<  error.message() << " " << error.value() << std::endl;
                if(!socket_.lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    this->getInfoFile(self);

                }
                // se l'invio del KO non va a buon fine potrei chiudere la connessione
                // todo: gestire errore
//                        else this->sendKORespAndClose();
            });
}

void Session::sendKORespAndClose(std::shared_ptr<Session> self) {
    std::string buffer = "KO\\\n";
    std::cout << "sendKORespAndClose" << std::endl;
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this, self](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "Chiudo la connessione per un errore" << std::endl;
            });
}

void Session::setUsername(std::string u) {
    std::unique_lock lock(Session::subscribers_mutex);
    Session::subscribers_.insert(u);
    lock.unlock();
    this->username = std::move(u);
}

// genero un nome temporaneo per il file dato da tempo corrente + id thread
std::string Session::tempFilePath(){
    std::filesystem::create_directory("temp/");
    auto th_id = std::this_thread::get_id();
    std::time_t t = std::time(nullptr);   // get time now
    std::stringstream ss;
    ss << "temp/" << th_id << "-" << t;
    return ss.str();
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
    std::cout << user_path.parent_path() << std::endl;
}

void Session::deleteFile(const std::shared_ptr<SyncedFileServer>& sfp){
    std::filesystem::path user_path("./users_files/");
    user_path += username;
    std::filesystem::path path(sfp->getPath());
    user_path += path;
    std::cout << "oooo" << std::endl;

    if(std::filesystem::is_directory(user_path) || std::filesystem::is_regular_file(user_path))
        std::filesystem::remove(user_path);
    std::cout << user_path << " erased" << std::endl;
}

void Session::deleteFileMap(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp){
    std::cout << "Elimino il file " << sfp->getPath() << std::endl;
    // todo: cosa succede se la mappa non contiene il file? genera eccezioni?
    user_map->erase(sfp->getPath());
    deleteFile(sfp);
    saveMap();
    this->sendOKRespAndRestart(self);
    std::cout << "FILE OK" << std::endl;
}

void Session::getFileEnd(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp, const std::string& tempPath){
    // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
    if(SyncedFileServer::CalcSha256(tempPath) == sfp->getHash()){
        std::cout << "sendo un ok" << std::endl;

        std::cout << "FILE OK (" << tempPath << " - " << sfp->getHash() << ')' << std::endl;
        // todo: gestire eccezioni della moveFile => ko in caso di errore
        moveFile(sfp, tempPath);
        // aggiorno il valore della mappa
        (*user_map)[sfp->getPath()] = sfp;
        saveMap();
        this->sendOKRespAndRestart(self);
    }
        // altrimenti comunico il problema al client che gestirà l'errore
    else {
        std::cout << "sendo un ko" << std::endl;

        std::cout << "FILE K0(" << tempPath<< "): hash errato(" << SyncedFileServer::CalcSha256(tempPath) << ')' << std::endl;
        // todo: elimino il file
        std::filesystem::remove(tempPath);
        this->sendKORespAndRestart(self);
    }
};

void Session::getFileR(
        std::shared_ptr<Session> self,
        const std::shared_ptr<SyncedFileServer>& sfp,
        std::shared_ptr<std::ofstream> file_ptr,
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
    std::cout << "Continuo il trasferimento del file" << std::endl;
    const ssize_t size_left = sfp->getFileSize()-sizeRead;
    std::cout << "size_left: " <<  size_left << std::endl;

    // scelgo la dimensione massima del buffer
    const ssize_t buff_size = size_left<N ? size_left: N;
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(data_, buff_size),
            [this, self, sfp, sizeRead, file_ptr, filePath](
                    const boost::system::error_code& error, // Result of operation.
                    std::size_t bytes_transferred           // Number of bytes copied into the buffer
            ){

                std::cout << "getFileR: " <<  error.message() << " " << error.value() << std::endl;

                if(!file_ptr->is_open()){
                    std::cout << "File not opened: " << filePath << std::endl;
                }
                if(!error){
                    std::cout << "bytes_transferred: " <<  bytes_transferred << std::endl;
                    // se non si sono verificati errori scrivo il file
                    data_[bytes_transferred] = '\0';
                    file_ptr->write(data_, bytes_transferred);
                    this->getFileR(self, sfp, file_ptr, filePath, sizeRead+bytes_transferred);
                } else {
                    file_ptr->close();
                    std::filesystem::remove(filePath);
                    this->sendKORespAndRestart(self);
                    // altrimenti gestisco l'errore
                }
                // todo: gestire errore e eccezione in caso di write fallita
            });
}


void Session::getFile(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp){
    // creo un nome file provvisorio univoco tra tutti i thread
    std::string filePath(tempFilePath());

    // apro il file
    auto file_ptr = std::make_shared<std::ofstream>(filePath, std::ios::binary);
    // todo: gestire eccezione in caso di open file fallita

    // avvio la callback ricorsiva
    this->getFileR(self, sfp, file_ptr, filePath, 0);
}

// richiedo il file solo se non è già presente o è diverso
void Session::sendNORespAndGetFile(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp) {
    // ogni thread lavora solo in lettura sulla mappa totale e lettura e scrittura sulle figlie => la mappa figlia è usata solo da un thread per volta
    // todo: eliminare tutti i file in caso di eccezione

    auto map_value = user_map->find(sfp->getPath());
    if(map_value == user_map->end() || map_value->second->getHash() != sfp->getHash()){
        // chiedo al client di mandarmi il file perchè non è presente
        std::cout << "sendo un no" << std::endl;

        std::string buffer = "NO\\\n";
        boost::asio::async_write(
                socket_,
                boost::asio::buffer(buffer, buffer.size()),
                [this, sfp, self](const boost::system::error_code& error, std::size_t bytes_transferred){
                    std::cout << "sendNORespAndGetFile: " <<  error.message() << " " << error.value() << std::endl;

                    if(!error){
                        std::cout << "FILE NO" << std::endl;
                        this->getFile(self, sfp);
                    } else this->sendKORespAndClose(self);
                });
    }else {
        std::cout << "sendo un ok" << std::endl;

        // il file è già presente quindi devo solo mandare un OK
        this->sendOKRespAndRestart(self);
    }
}

void Session::clearBuffer(){
    boost::asio::socket_base::bytes_readable command(true);
    socket_.lowest_layer().io_control(command);
    std::size_t bytes_readable = command.get();
    std::cout << "Il buffer contiene " << bytes_readable << "bytes" << std::endl;
    std::string data;
    ssize_t bytes_read = boost::asio::read(socket_, boost::asio::buffer(data, bytes_readable));
    std::cout << "Ho svuotato il buffer " << bytes_read << std::endl;
}

void Session::getInfoFile(std::shared_ptr<Session> self) {
    // attendo un json con le informazioni sul file
    boost::asio::async_read_until(
            socket_,
            this->buf,
            "\\\n",
            [this, self](const boost::system::error_code& error, std::size_t bytes_transferred){
                std::cout << "getInfoFile: " <<  error.message() << " " << error.value() << std::endl;
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(this->buf.data());
                        buf.consume(bytes_transferred);
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string json = data.substr(0, bytes_transferred-2);
                        std::cout << json << "size: " << bytes_transferred << std::endl;
                        std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(json);
                        switch (sfp->getFileStatus()) {
                            case FileStatus::created:
                                std::cout << "created" << std::endl;
                                sendNORespAndGetFile(self, sfp);
                                break;
                            case FileStatus::modified:
                                std::cout << "modified" << std::endl;
                                sendNORespAndGetFile(self, sfp);
                                break;
                            case FileStatus::erased:
                                std::cout << "erased" << std::endl;
                                deleteFileMap(self, sfp);
                                break;
                            case FileStatus::not_valid:
                                std::cout << "not valid" << std::endl;
                                this->sendKORespAndRestart(self);
                                break;
                        }
                    } catch (std::runtime_error &exception) {
                        // todo: se si verifica un errore sul filesystem forse dovrei chiudere il client anche se in questo caso l'errore può essere solo sulla delete
                        // se si verifica un errore potrebbe essere dovuto dal filesystem o da un errore sui dati
                        // loggo l'errore e riavvio il lavoro dopo avere mandato un KO al client
                        std::cout << exception.what() << std::endl;
                        this->clearBuffer();
                        this->sendKORespAndRestart(self);
                    }
                } else this->sendOKRespAndRestart(self);
            });
}

void Session::setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap){
    this->user_map = std::move(userMap);
}

Session::~Session() {
    std::cout << "Sessione distrutta" << std::endl;
    std::unique_lock lock(Session::subscribers_mutex);
    // todo: cosa succede se l'elemento non esiste
    Session::subscribers_.erase(this->username);
}

std::shared_ptr<Session> Session::create(tcp::socket socket, boost::asio::ssl::context &context) {
    return std::make_shared<Session>(std::move(socket), context);
}

bool Session::isLogged(const std::string& username) {
    std::shared_lock lock(Session::subscribers_mutex);
    auto it = Session::subscribers_.find(username);
    return it!=Session::subscribers_.end();
}


std::set<std::string> Session::subscribers_;
std::shared_mutex Session::subscribers_mutex;


