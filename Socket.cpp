//
// Created by stefano on 07/08/20.
//

#include "Socket.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <fstream>
#include <utility>

Socket::Socket(int sockfd) : socket_fd(sockfd) {
    std::cout << "Socket " << sockfd << " created" << std::endl;
}

Socket::Socket(){
    socket_fd = ::socket(AF_INET, SOCK_STREAM, 0); //solo tcp ipv4
    if (socket_fd < 0) throw std::runtime_error("Cannot create socket");
    std::cout << "Socket" << socket_fd << " created" << std::endl;
}
Socket::~Socket() {
    if (socket_fd != 0) {
        std::cout << "Socket " << socket_fd << " closed" << std::endl;
        ::close(socket_fd);
    }
}

Socket::Socket(Socket &&other) noexcept : socket_fd(other.socket_fd){  // costruttore di movimento
    other.socket_fd = 0;
}

Socket& Socket::operator=(Socket &&other) noexcept {   // costruttore di copia per movimento
    if(socket_fd != 0) close(socket_fd);
    socket_fd = other.socket_fd;
    other.socket_fd = 0;
    return *this;
}

ssize_t Socket::read(char *buffer, size_t len){
    ssize_t res = recv(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw std::runtime_error("Cannot receive from socket");
    return res;
}

ssize_t Socket::write(const char *buffer, size_t len){
    ssize_t res = send(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw std::runtime_error("Cannot write to socket");
    return res;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len){
    if(::connect(socket_fd, reinterpret_cast<struct sockaddr*>(addr), len) != 0)
        throw std::runtime_error("Cannot connect to remote socket");
}

void Socket::connectToServer(std::string address, int port) {
    //todo: controllare che addr non venga distrutto
    struct sockaddr_in addr;
    inet_pton(AF_INET, address.c_str(), &(addr.sin_addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    this->connect(&addr, sizeof(addr));
}

void Socket::closeConnection() {
    ::close(socket_fd);
}

void Socket::sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile) {
    char buffer[N];
    unsigned long size_read = 0;
    const bool isFile = syncedFile->isFile() && syncedFile->getFileStatus()!=FileStatus::erased;
//    syncedFile->update_file_data();
    std::ifstream file_to_send(syncedFile->getPath(), std::ios::binary);
    if(isFile) {
        // todo: se apro il file l'os dovrebbe impedire la modifica agli altri programmi?
        // todo: se il file non esiste più cosa faccio? Lo marchio come eliminato o ignoro il problema visto che alla prossima scansione si accorgerà che il file è stato eliminato?
        file_to_send = std::ifstream(syncedFile->getPath(), std::ios::binary);
        // se il file non esiste ignoro il problema ed esco, alla prossima scansione del file system verrà notata la sua assenza
        if (!file_to_send.is_open())
            return;
        std::cout << "Size to read: " << file_to_send.tellg() << std::endl;
    }
    std::string json = syncedFile->getJSON();
    // invio il json
    // todo: gestire il timeout a livello write and read con delle eccezioni
    this->write(json.c_str(), json.size());

    // todo: posso implementare una risposta del server, se il file è già presente non devo rinviare il file

    if(isFile) {
        std::cout << "Invio il file" << std::endl;
        // forse questa if è da togliere, se il file era aperto prima, deve per forza essere aperto anche ora
        if (!file_to_send.is_open())
            std::cout << "File closed"<< std::endl;
        // leggo e invio il file
        std::cout << "Size to read: " << file_to_send.tellg() << std::endl;
        unsigned long size = 1;
        while (size_read < syncedFile->getFileSize() && size>0) {
            file_to_send.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            unsigned long size = file_to_send.gcount();
            this->write(buffer, size);
            size_read += size;
            std::cout << "Size read: " << size_read << std::endl;

        }
        file_to_send.close();
    }

//    if(syncedFile->isFile()) {
//        // leggo e invio il file
//        // todo: se apro il file l'os dovrebbe impedire la modifica agli altri programmi?
//        std::ifstream file_to_send(syncedFile->getPath(), std::ios::binary);
//        if (!file_to_send.is_open())
//            throw std::runtime_error("Cannot open the file");
//        std::cout << "Size to read: " << file_to_send.tellg() << std::endl;
//        unsigned long size = 1;
//        while (size_read < syncedFile->getFileSize() && size>0) {
//            file_to_send.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
//            size = file_to_send.gcount();
//            this->write(buffer, size);
//            size_read += size;
//        }
//        file_to_send.close();
//    }
}

std::string Socket::readJSON() {
    char buffer[N];
    ssize_t l = 0;
    // todo: leggere fino a quando non si incontrano i caratteri "} o qualcosa di simile
    l+= this->read(buffer+l, N-l);
    std::cout << "read size: " << l << std::endl;
//    while (this->read(buffer+l, N-l)>0);
    return buffer;
}

void Socket::askFile() {

}

std::string Socket::getFile() {
    return std::string();
}

void Socket::fileError() {

}

const std::string &Socket::getUsername() const {
    return username;
}

void Socket::setUsername(std::string u) {
    this->username = std::move(u);
}



