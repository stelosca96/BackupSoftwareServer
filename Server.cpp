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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <utility>

namespace pt = boost::property_tree;

void Server::loadUsers(){
    std::cout << "Load users" << std::endl;
    std::ifstream infile(file_users);
    std::string username, hash, salt;
    while (infile >> username >> hash >> salt){
        std::cout << "U: " << username << " P: " << hash << std::endl;
        users[username] = User(username, hash, salt);
    }
}

void Server::saveUsername(const User& user){
    // il salvataggio degli utenti è gestito solo dal thread principale quindi non ho bisogno di sincronizzazione
    std::cout << "Save user" << std::endl;
    // todo: gestire eccezione
    std::ofstream outfile;
    outfile.open(file_users, std::ios_base::app); // append instead of overwrite
    outfile << user.getUsername() << ' ' << user.getHashedPassword() << ' ' << user.getSalt() << std::endl;
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
            // se non riesco a caricare la mappa per un utente, quando il client noterà
            // un cambiamento un file, il server se lo farà rinviare anche se già presente,
            // considerandolo come non presente
            std::cout << error.what() << std::endl;
        }
    }
}

bool Server::auth(User& user){
    std::unique_lock lock(mutex_users);
    auto map_user = users.find(user.getUsername());
    if (map_user == users.end()){
        // l'username deve essere lungo almeno 3 caratteri
        if(user.getUsername().length()<3)
            return false;
        // l'username non deve contenere spazi o / o \ e valutare altri caratteri speciali
        for(char i : user.getUsername())
            for(char k: forbiddenChars)
                if(i==k)
                    return false;
        if(Session::isLogged(user.getUsername()))
            return false;
        const std::string& username = user.getUsername();
        user.calculateHash();
        users[username] = user;
        create_empty_map(username);
        saveUsername(user);
        return true;
    }
    if(Session::isLogged(user.getUsername()))
        return false;
    user.setSalt(map_user->second.getSalt());
    return map_user->second.getHashedPassword() == user.getHashedPassword();
}


std::string Server::get_password() {
    return cert_password;
}

void Server::do_accept() {
    acceptor_.async_accept(
            [this](const boost::system::error_code& error, tcp::socket socket){
                std::cout << "do_accept: " <<  error.message() << std::endl;
                if(!socket.lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if (!error){
                    auto session = Session::create(std::move(socket), context_);
//                    sessions.push_back(session);
                    // con la sessione tcp appena generata effettuo l'handshake
                    this->do_handshake(session);

                    // e mi rimetto in ascolto per l'accept
                    this->do_accept();
                }
                // se l'accept non va a buon fine la sesione tcp verrà chiusa
            });
}

void Server::do_handshake(const std::shared_ptr<Session>& session){
    std::cout << "do_handshake start" << std::endl;
    // effettuo l'handshake per la connessione ssl
    session->getSocket().async_handshake(
            boost::asio::ssl::stream_base::server,
            [this, session](const boost::system::error_code& error){
                std::cout << "do_handshake: " <<  error.message() << std::endl;
                if(!session->getSocket().lowest_layer().is_open()){
                    // se la connesione è stata chiusa dal client termino il lavoro
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    // se l'handshake va a uon fine
                    this->do_auth(session);
                }
                // se l'handshake non va a bon fine la sesione tcp verrà chiusa
            }
    );
}

void Server::do_auth(const std::shared_ptr<Session>& session){
    boost::asio::async_read_until(
            session->getSocket(),
            session->buf,
            "\\\n",
            [this, session](
                    const boost::system::error_code& error,
                    std::size_t bytes_transferred           // Number of bytes written from the
            ){
                std::cout << "do_auth: " <<  error.message() << std::endl;
                if(!session->getSocket().lowest_layer().is_open()){
                    std::cout << "connessione chiusa" << std::endl;
                    return;
                }
                if(!error){
                    try {
                        std::string data = boost::asio::buffer_cast<const char*>(session->buf.data());
                        session->buf.consume(bytes_transferred);
                        // rimuovo i terminatori quindi gli ultimi due caratteri
                        std::string json = data.substr(0, data.length()-2);
                        std::cout << data << "size: " << bytes_transferred << std::endl;

                        // creo un oggetto user a partire dal json ed effettuo l'autenticazione
                        User user(json);
                        if(auth(user)){
                            session->setUsername(user.getUsername());
                            // todo: devo mantenere un lista aggiornata con lo stato delle sessioni tcp aperte
                            std::shared_lock lock(mutex_map);
                            session->setMap(synced_files[user.getUsername()]);
                            lock.unlock();
                            session->sendOKRespAndRestart(session);
                        }
                        else session->sendKORespAndClose(session);
                    } catch(std::runtime_error& e) {
                        std::cout << e.what() << std::endl;
                        // questa eccezione è generata da un'errore sul parsing dei dati di auth o del file system,
                        // quindi chiudo la connessione dopo avere notificato il client
                        session->sendKORespAndClose(session);
                    }
                }
            });
}


Server::Server(
        boost::asio::io_context &io_context,
        unsigned short port,
        const std::string& crt,
        const std::string& key,
        std::string cert_password,
        const std::string& dhTmp):
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        context_(boost::asio::ssl::context::tlsv13_server),
        cert_password(std::move(cert_password)){
    // todo: gestire un massimo numero di conessioni
    // uso tls 1.3, vieto ssl2 e 3, dh per generare una key va rieffettuato ad ogni connessione, Implement various bug workarounds.
    context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
    context_.set_password_callback(std::bind(&Server::get_password, this));
    context_.use_certificate_chain_file(crt);
    context_.use_private_key_file(key, boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file(dhTmp);

    try {
        loadUsers();
        try {
            loadMaps();
        } catch (filesystemException &exception) {
            // todo: se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
            // cosa succede se l'utente è presente e la mappa no? penso nulla di grave, lo notifica solamente
            std::cout << exception.what() << std::endl;
        }
    } catch (filesystemException &exception) {
        //se si verifica un errore durante la lettura devo partire da una mappa vuota e aprire un nuovo file
        std::cout << exception.what() << std::endl;
    }
}

