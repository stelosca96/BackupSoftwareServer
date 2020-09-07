//
// Created by stefano on 30/08/20.
//

#ifndef SERVERBACKUP_SESSION_H
#define SERVERBACKUP_SESSION_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "SyncedFileServer.h"
#include <boost/asio/buffer.hpp>
#include <set>
#include <shared_mutex>
#include "Server.h"
using boost::asio::ip::tcp;
typedef boost::asio::ssl::stream<tcp::socket> ssl_socket;

class Session: public std::enable_shared_from_this<Session> {
private:
    const static int N = 10240;
    static std::set<std::string> subscribers_;
    static std::shared_mutex subscribers_mutex;
    std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> user_map;
    tcp::socket socket_;
    std::string username;
    boost::asio::streambuf buf;
    char data_[N+1];

    void saveMap();
    void sendKORespAndRestart(std::shared_ptr<Session> session);
    void sendNORespAndGetFile(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp);
    void getFile(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer>& sfp);
    void getInfoFile(const std::shared_ptr<Session>& session);

    static std::string tempFilePath();

    void getFileEnd(
            std::shared_ptr<Session> self,
            const std::shared_ptr<SyncedFileServer>& sfp,
            const std::string& filePath);

    void getFileR(
            std::shared_ptr<Session> self,
            const std::shared_ptr<SyncedFileServer>& sfp,
            std::shared_ptr<std::ofstream> file_ptr,
            const std::string& filePath,
            ssize_t sizeRead
    );

    void moveFile(const std::shared_ptr<SyncedFileServer> &sfp, const std::string &tempPath);

    void deleteFile(const std::shared_ptr<SyncedFileServer> &sfp);

    void deleteFileMap(std::shared_ptr<Session> self, const std::shared_ptr<SyncedFileServer> &sfp);

    tcp::socket &getSocket();
    void clearBuffer();

    friend class Server;
public:

    ~Session();
    void sendOKRespAndRestart(std::shared_ptr<Session> session);
    void sendKORespAndClose(std::shared_ptr<Session> self);
    void setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap);
    void setUsername(std::string username);
    static bool isLogged(const std::string& username);
    static std::shared_ptr<Session> create(tcp::socket socket);
    Session(
            tcp::socket socket
    );

};


#endif //SERVERBACKUP_SESSION_H
