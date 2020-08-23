//
// Created by stefano on 07/08/20.
//

#ifndef FILEWATCHER_SOCKET_H
#define FILEWATCHER_SOCKET_H


#include <cstdio>
#include <string>
#include <memory>
#include "SyncedFileServer.h"

class Socket {
private:
    //todo: mettere una dimensione del buffer ragionevole
    const static int N = 10240;
    int socket_fd;
    const static int timeout_value = 5;

    std::string username;
    Socket(int sockfd);
    ssize_t read(char *buffer, size_t len) const;
    ssize_t write(const char *buffer, size_t len) const;
    void connect(struct sockaddr_in *addr, unsigned int len) const;
    void sendString(const std::string &str);
    static int Select(int max_fd, fd_set *read_set, fd_set *write_set, fd_set *except_set, struct timeval *timeout);
    void setSelect(fd_set &fdSet, timeval &timeout);
    static std::string tempFilePath();

    friend class ServerSocket; //è friend perché voglio poter chiamare il suo costruttore privato

public:
    Socket &operator=(const Socket &) = delete; //elimino operatore di assegnazione
    Socket();
    ~Socket();
    Socket(const Socket &) = delete; //elimino il costruttore di copia
    Socket(Socket &&other) noexcept ;  // costruttore di movimento

    Socket &operator=(Socket &&other) noexcept ; // costruttore di copia per movimento
    void connectToServer(std::string address, int port);
    void closeConnection() const;
    void sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile);
    std::string getJSON();
    std::string getFile(unsigned long size);
    void fileError();

    [[nodiscard]] const std::string &getUsername() const;
    void setUsername(std::string username);


    void sendOKResp();
    void sendKOResp();
    void sendNOResp();
    std::string getResp();

    void sendJSON(const std::string& JSON);

    bool sockReadIsReady();


    void clearReadBuffer();
};


#endif //FILEWATCHER_SOCKET_H
