//
// Created by stefano on 30/08/20.
//

#ifndef SERVERBACKUP_SESSION_H
#define SERVERBACKUP_SESSION_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "SyncedFileServer.h"
#include <boost/asio/buffer.hpp>
#include "Server.h"
using boost::asio::ip::tcp;
typedef boost::asio::ssl::stream<tcp::socket> ssl_socket;

class Session {
private:
    const static int N = 10240;
    std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> user_map;
    ssl_socket socket_;
    std::string username;
    boost::asio::streambuf buf;
    char data_[N+1];

    void saveMap(const std::string& username);

    boost::asio::ssl::stream<tcp::socket> &getSocket();
    friend class Server;


public:
    Session(
            tcp::socket socket,
            boost::asio::ssl::context& context
    );
    ~Session();
    void sendOKRespAndRestart();
    void sendKORespAndRestart();
    void sendKORespAndClose();
    void sendNORespAndGetFile(const std::shared_ptr<SyncedFileServer>& sfp);
    void getInfoFile();
    void setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap);

    void setUsername(std::string username);

    void getFile(const std::shared_ptr<SyncedFileServer>& sfp);

    static std::string tempFilePath();

    void getFileEnd(const std::shared_ptr<SyncedFileServer>& sfp, const std::string& filePath);

    void getFileR(
            const std::shared_ptr<SyncedFileServer>& sfp,
            std::shared_ptr<std::ofstream> file_ptr,
            const std::string& filePath,
            ssize_t sizeRead
    );

    void moveFile(const std::shared_ptr<SyncedFileServer> &sfp, const std::string &tempPath);

    void deleteFile(const std::shared_ptr<SyncedFileServer> &sfp);
};


#endif //SERVERBACKUP_SESSION_H
