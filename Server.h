//
// Created by stefano on 30/08/20.
//

#ifndef SERVERBACKUP_SERVER_H
#define SERVERBACKUP_SERVER_H

#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "User.h"
#include "Session.h"
#include "SyncedFileServer.h"
#include <shared_mutex>

using boost::asio::ip::tcp;

class Server{
private:
    tcp::acceptor acceptor_;
    boost::asio::ssl::context context_;

    const std::string file_users = "users_list.conf";
    const std::string cert_password = "ciao12345";
    // todo: non permettere nomi che contengano caratteri speciali non possibili su windows
    std::vector<char> forbiddenChars = {' ', '/', '\\'};
    std::shared_mutex mutex_map;
    std::mutex mutex_users;

    // struttura dati condivisa
    std::unordered_map<std::string, User> users;

    // struttura dati condivisa
    std::unordered_map<std::string, std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>>> synced_files;

    // struttura dati condivisa
    std::vector<std::shared_ptr<Session>> sessions;

    void loadUsers();
    void saveUsername(const User& user);
    bool auth(User& user);
    std::string get_password() ;
    void create_empty_map(const std::string& username);
    void loadMap(const std::string& username);
    void loadMaps();
public:
    Server(boost::asio::io_context& io_context, unsigned short port, unsigned int max_sockets);
    void do_accept();

    void do_handshake(const std::shared_ptr<Session> &session);

    void do_auth(const std::shared_ptr<Session> &session);

};


#endif //SERVERBACKUP_SERVER_H
