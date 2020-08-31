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
#include "Jobs.h"
#include <shared_mutex>

using boost::asio::ip::tcp;

class Server: boost::enable_shared_from_this<Server>{
private:
    tcp::acceptor acceptor_;
    boost::asio::ssl::context context_;
    std::unordered_map<std::string, User> users;
    const std::string file_users = "users_list.conf";
    std::shared_mutex mutex_map;
    std::unordered_map<std::string, std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>>> synced_files;
    std::vector<std::shared_ptr<Session>> sessions;
    //    Jobs jobs;

    void loadUsers();
    void saveUsername(const User& user);
    bool auth(const User& user);
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
