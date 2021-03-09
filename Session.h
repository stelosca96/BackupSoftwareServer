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
#include "protocolMode.h"
using boost::asio::ip::tcp;
typedef boost::asio::ssl::stream<tcp::socket> ssl_socket;

class Session: public std::enable_shared_from_this<Session> {
private:
    const static int N = 10240;
    unsigned retry_count;
    static std::set<std::string> subscribers_;
    static std::shared_mutex subscribers_mutex;
    std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> user_map;
    ssl_socket socket_;
    std::string username;
    boost::asio::streambuf buf;
    char data_[N+1];
    ProtocolMode mode;
    std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>::iterator next_file;

    void saveMap();
    void sendKORespAndRestart(const std::shared_ptr<Session>& session);
    void sendNORespAndGetFile(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp);
    void getFile(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer>& sfp);
    void getInfoFile(const std::shared_ptr<Session>& session);

    void sendInfoFile(const std::shared_ptr<Session>& session);
    void sendJSONFileR(const std::shared_ptr<Session>& session);
    void getResp(const std::shared_ptr<Session>& session);
    void sendBinaryFile(const std::shared_ptr<Session>& session);
    void sendBinaryFileR(const std::shared_ptr<Session>& session, const std::shared_ptr<std::ifstream>& file_to_send);
    void sendEndRestoreAndClose(const std::shared_ptr<Session>& session);
    static std::string tempFilePath();

    void getFileEnd(
            const std::shared_ptr<Session>& self,
            const std::shared_ptr<SyncedFileServer>& sfp,
            const std::string& filePath);

    void getFileR(
            const std::shared_ptr<Session>& self,
            const std::shared_ptr<SyncedFileServer>& sfp,
            const std::shared_ptr<std::ofstream>& file_ptr,
            const std::string& filePath,
            ssize_t sizeRead
    );


    void moveFile(const std::shared_ptr<SyncedFileServer> &sfp, const std::string &tempPath);

    void deleteFile(const std::shared_ptr<SyncedFileServer> &sfp);

    void deleteFileMap(const std::shared_ptr<Session>& self, const std::shared_ptr<SyncedFileServer> &sfp);

    boost::asio::ssl::stream<tcp::socket> &getSocket();
    void clearBuffer();
    void getMode(const std::shared_ptr<Session>& session);


    friend class Server;
public:

    ~Session();
    void sendOKRespAndRestart(const std::shared_ptr<Session>& session);
    void sendKORespAndClose(std::shared_ptr<Session> self);
    void setMap(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<SyncedFileServer>>> userMap);
    void setUsername(std::string username);
    static bool isLogged(const std::string& username);
    static std::shared_ptr<Session> create(tcp::socket socket, boost::asio::ssl::context &context);
    Session(
            tcp::socket socket,
            boost::asio::ssl::context& context
    );

};


#endif //SERVERBACKUP_SESSION_H
