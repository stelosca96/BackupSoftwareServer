//
// Created by stefano on 07/08/20.
//

#ifndef FILEWATCHER_SOCKET_H
#define FILEWATCHER_SOCKET_H


#include <cstdio>
#include <string>
#include <memory>
#include "SyncedFileServer.h"

class string;

class Socket {
private:
    //todo: mettere una dimensione del buffer ragionevole
    const static int N = 10240;
    int socket_fd;
    std::string username;
    Socket(int sockfd);
    ssize_t read(char *buffer, size_t len);
    ssize_t write(const char *buffer, size_t len);
    void connect(struct sockaddr_in *addr, unsigned int len);
    friend class ServerSocket; //è friend perché voglio poter chiamare il suo costruttore privato

public:
    Socket &operator=(const Socket &) = delete; //elimino operatore di assegnazione
    Socket();
    ~Socket();
    Socket(const Socket &) = delete; //elimino il costruttore di copia
    Socket(Socket &&other) noexcept ;  // costruttore di movimento

    Socket &operator=(Socket &&other) noexcept ; // costruttore di copia per movimento
    void connectToServer(std::string address, int port);
    void closeConnection();
    void sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile);
    std::string readJSON();
    void askFile();
    std::string getFile();
    void fileError();

    const std::string &getUsername() const;

    void setUsername(std::string username);
};


#endif //FILEWATCHER_SOCKET_H
