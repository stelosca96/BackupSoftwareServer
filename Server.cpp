//
// Created by stefano on 30/08/20.
//

#include "Server.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include "exceptions/socketException.h"
#include "exceptions/dataException.h"
#include "exceptions/filesystemException.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <utility>

namespace pt = boost::property_tree;

void Server::loadUsers(){
    std::cout << "Load users" << std::endl;
    std::ifstream infile(file_users);
    std::string username, password;
    while (infile >> username >> password){
        std::cout << "U: " << username << " P: " << password << std::endl;
        users[username] = User(username, password);
    }
}

void Server::saveUsername(const User& user){
    // il salvataggio degli utenti è gestito solo dal thread principale quindi non ho bisogno di sincronizzazione
    std::cout << "Save user" << std::endl;
    // todo: gestire eccezione
    std::ofstream outfile;
    outfile.open(file_users, std::ios_base::app); // append instead of overwrite
    outfile << user.getUsername() << ' ' << user.getPassword() << std::endl;
    outfile.close();
}

void Server::create_empty_map(const std::string& username){
    std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>> user_map;
    std::unique_lock lock(mutex_map);
    synced_files[username] = std::make_shared<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>>(std::move(user_map));
}


void Server::loadMap(const std::string& username){
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

void Server::loadMaps(){
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

bool Server::auth(const User& user){
    //todo hashare e salare passwords
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


std::string Server::get_password() {
    return "ciao12345";
}

void Server::do_accept() {
    acceptor_.async_accept(
            [this](const boost::system::error_code& error, tcp::socket socket){
                std::cout << "do_accept: " <<  error.message() << std::endl;
                if (!error){
                    auto session = std::make_shared<Session>(std::move(socket), context_);
                    this->do_handshake(session);
                    this->do_accept();
                }
                // todo: se l'accept non va a buon fine cosa faccio? Chiudo il programma?
            });
}

void Server::do_handshake(const std::shared_ptr<Session>& session){
//    auto self = shared_from_this();
    session->getSocket().async_handshake(
            boost::asio::ssl::stream_base::server,
            [this, session](const boost::system::error_code& error){
                std::cout << "do_handshake: " <<  error.message() << std::endl;
                if(!error){
                    this->do_auth(session);
                }
                // se l'handshake non va a bon fine la sesione tcp verrà chiusa
            }
    );
}

void Server::do_auth(const std::shared_ptr<Session>& session){
    boost::asio::streambuf buf;
    boost::asio::async_read_until(
            session->getSocket(),
            buf,
            "\\\n",
            [this, &session, &buf](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "do_auth: " <<  error.message() << std::endl;
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(buf.data());
                        std::cout << data << std::endl;
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string json = data.substr(0, data.length()-2);
                        User user(json);
                        if(auth(user)){
                            session->setUsername(user.getUsername());
                            // todo: devo mantenere un lista aggiornata con lo stato delle sessioni tcp aperte
//                        sessions[user.getUsername()] = session;
                            session->setMap(synced_files[user.getUsername()]);
//                            session->sendOKRespAndRestart();

                        }
//                        else session->sendKORespAndClose();
                    } catch(std::exception& e) {
                        std::cout << e.what() << std::endl;
                    }
                }
            });
}


Server::Server(boost::asio::io_context &io_context, unsigned short port, unsigned int max_sockets):
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        context_(boost::asio::ssl::context::sslv23){
    // todo: gestire un massimo numero di sockets
    context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
    context_.set_password_callback(std::bind(&Server::get_password, this));
    context_.use_certificate_chain_file("/home/stefano/CLionProjects/test_ssl/user.crt");
    context_.use_private_key_file("/home/stefano/CLionProjects/test_ssl/user.key", boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file("/home/stefano/CLionProjects/test_ssl/dh2048.pem");

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
}

void Session::setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap){
    this->user_map = std::move(userMap);
}