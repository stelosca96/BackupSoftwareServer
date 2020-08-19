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
    const static int timeout_value = 20;

    std::string username;
    Socket(int sockfd);
    ssize_t read(char *buffer, size_t len) const;
    ssize_t write(const char *buffer, size_t len) const;
    void connect(struct sockaddr_in *addr, unsigned int len) const;
    bool setReadSelect(fd_set &read_fds);
    bool setWriteSelect(fd_set &write_fds);

    bool sendResp(std::string resp);

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
    bool sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile);
    std::optional<std::string> readJSON();
    std::optional<std::string> getFile(int size);
    void fileError();

    [[nodiscard]] const std::string &getUsername() const;
    void setUsername(std::string username);

    static std::string tempFilePath();

    bool sendOKResp();
    bool sendKOResp();
    bool sendNOResp();
    std::optional<std::string> getResp();

    bool sendJSON(const std::string& JSON);
};


#endif //FILEWATCHER_SOCKET_H
