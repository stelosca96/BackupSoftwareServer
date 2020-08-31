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
#include "Session.h"

namespace pt = boost::property_tree;

void Session::saveMap(const std::string& username){
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

void Session::sendOKRespAndRestart() {
    std::string buffer = "OK\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "sendOKRespAndRestart: " <<  error.message() << std::endl;
                if(!error){
                    this->getInfoFile();
                }
                // toodo: gestire errore
//                        else this->sendKORespAndClose();
            });
}

void Session::sendKORespAndRestart() {

}

void Session::sendKORespAndClose() {
    std::string buffer = "KO\\\n";
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(buffer, buffer.size()),
            [this](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                if(!error){
                    this->getInfoFile();
                }
                // toodo: gestire errore
//                        else this->sendKORespAndClose();
            });
//            [this](const boost::system::error_code& error){
//                if(!error){
//                    this->getInfoFile();
//                } else this->sendKORespAndClose();
//            })
//            ;
}

void Session::setUsername(std::string u) {
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

//void deleteFileMap(const std::shared_ptr<SyncedFileServer>& sfp){
//    std::cout << "Qua" << std::endl;
//
//    if(synced_files.find(sfp->getPath())!=synced_files.end()){
//        std::cout << "Quo" << std::endl;
//
//        std::shared_lock map_lock(mutex_map);
//        auto user_map = synced_files.find(sock->getUsername())->second;
//        map_lock.unlock();
//        user_map->erase(sfp->getPath());
//    }
//    std::cout << "Qui" << std::endl;
//    deleteFile(sock->getUsername(), sfp);
//    sock->sendOKResp();
//    std::cout << "FILE OK" << std::endl;
//}

void Session::getFileEnd(const std::shared_ptr<SyncedFileServer>& sfp, const std::string& tempPath){
    // controllo che il file ricevuto sia quello che mi aspettavo e che non ci siano stati errori
    if(SyncedFileServer::CalcSha256(tempPath) == sfp->getHash()){
        std::cout << "sendo un ok" << std::endl;

        std::cout << "FILE OK (" << tempPath << " - " << sfp->getHash() << ')' << std::endl;
        // todo: gestire eccezioni della moveFile => ko in caso di errore
        moveFile(sfp, tempPath);
        // aggiorno il valore della mappa
        (*user_map)[sfp->getPath()] = sfp;
        this->sendKORespAndRestart();
    }
        // altrimenti comunico il problema al client che gestirà l'errore
    else {
        std::cout << "sendo un ko" << std::endl;

        std::cout << "FILE K0(" << tempPath<< "): hash errato(" << SyncedFileServer::CalcSha256(tempPath) << ')' << std::endl;
        // todo: elimino il file
        std::filesystem::remove(tempPath);
        this->sendKORespAndRestart();
    }
};

void Session::getFileR(
        const std::shared_ptr<SyncedFileServer>& sfp,
        std::shared_ptr<std::ofstream> file_ptr,
        const std::string& filePath,
        ssize_t sizeRead){
    // il file che ricevo è già aperto, ma verifico che lo sia
    if(!file_ptr->is_open()){
        std::cout << "File not opened: " << filePath << std::endl;
        this->sendKORespAndRestart();
    }
    if(sizeRead>=sfp->getFileSize()){
        std::cout << "Ho terminato il trasferimento del file" << std::endl;
        file_ptr->close();
        this->getFileEnd(sfp, filePath);
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
            [this, sfp, sizeRead, file_ptr, filePath](
                    const boost::system::error_code& error, // Result of operation.
                    std::size_t bytes_transferred           // Number of bytes copied into the
            ){

                std::cout << "getFileR: " <<  error.message() << std::endl;

                if(!file_ptr->is_open()){
                    std::cout << "File not opened: " << filePath << std::endl;
                }
                if(!error){
                    std::cout << "bytes_transferred: " <<  bytes_transferred << std::endl;
                    // se non si sono verificati errori scrivo il file
                    data_[bytes_transferred] = '\0';
                    file_ptr->write(data_, bytes_transferred);
                    this->getFileR(sfp, file_ptr, filePath, sizeRead+bytes_transferred);
                } else {
                    // altrimenti gestisco l'errore
                }
            });
}


void Session::getFile(const std::shared_ptr<SyncedFileServer>& sfp){
    // creo un nome file provvisorio univoco tra tutti i thread
    std::string filePath(tempFilePath());

    // apro il file
    auto file_ptr = std::make_shared<std::ofstream>(filePath, std::ios::binary);
    // avvio la callback ricorsiva
    this->getFileR(sfp, file_ptr, filePath, 0);
}

// richiedo il file solo se non è già presente o è diverso
void Session::sendNORespAndGetFile(const std::shared_ptr<SyncedFileServer>& sfp) {
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
                [this, sfp](
                        const boost::system::error_code& error,
                        std::size_t bytes_transferred           // Number of bytes written from the
                ){
                    std::cout << "sendNORespAndGetFile: " <<  error.message() << std::endl;

                    if(!error){
                        std::cout << "FILE NO" << std::endl;
                        this->getFile(sfp);
                    } else this->sendKORespAndClose();
                });
    }else {
        std::cout << "sendo un ok" << std::endl;

        // il file è già presente quindi devo solo mandare un OK
        this->sendOKRespAndRestart();
    }
}

void Session::getInfoFile() {
    boost::asio::async_read_until(
            socket_,
            this->buf,
            "\\\n",
            [this](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "getInfoFile: " <<  error.message() << std::endl;
                if(!error){
                    // todo: gestire errore
                    // terminate called after throwing an instance of 'boost::wrapexcept<boost::property_tree::ptree_bad_path>'
                    std::string data = boost::asio::buffer_cast<const char*>(this->buf.data());
                    buf.consume(bytes_transferred);
                    // rimuovo i terminatori quindi gli ultimi due caratteri
                    std::cout << data << "size: " << bytes_transferred << std::endl;
                    std::string json = data.substr(0, data.length()-2);
                    std::shared_ptr<SyncedFileServer> sfp = std::make_shared<SyncedFileServer>(json);
                    switch (sfp->getFileStatus()){
                        case FileStatus::created:
                            std::cout << "created" << std::endl;
                            sendNORespAndGetFile(sfp);
                            break;
                        case FileStatus::modified:
                            std::cout << "modified" << std::endl;
                            sendNORespAndGetFile(sfp);
                            break;
                        case FileStatus::erased:
                            std::cout << "erased" << std::endl;
//                            deleteFileMap(sfp, sock);
                            break;
                        case FileStatus::not_valid:
                            std::cout << "not valid" << std::endl;
                            this->sendKORespAndRestart();
                            break;
                    }
                } else this->sendKORespAndClose();
            });
}

Session::~Session() {
    std::cout << "Sessione distrutta" << std::endl;
}
