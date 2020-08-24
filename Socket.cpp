//
// Created by stefano on 07/08/20.
//

#include "Socket.h"
#include "exceptions/socketException.h"
#include "exceptions/dataException.h"
#include "exceptions/filesystemException.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <fstream>
#include <utility>
#define INTERRUPTED_BY_SIGNAL (errno == EINTR)
// todo: valutare se utile https://www.codeproject.com/Articles/1264257/Socket-Programming-in-Cplusplus-using-boost-asio-T

Socket::Socket(int sock_fd) : socket_fd(sock_fd) {
    std::cout << "Socket " << sock_fd << " created" << std::endl;
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

Socket::Socket(Socket &&other) noexcept : socket_fd(other.socket_fd), username(other.username){  // costruttore di movimento
    other.socket_fd = 0;
}

Socket& Socket::operator=(Socket &&other) noexcept {   // costruttore di copia per movimento
    if(socket_fd != 0) close(socket_fd);
    socket_fd = other.socket_fd;
    username = other.username;
    other.socket_fd = 0;
    return *this;
}

ssize_t Socket::read(char *buffer, size_t len) const{
    ssize_t res = recv(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw socketException("Cannot receive from socket");
    return res;
}

ssize_t Socket::write(const char *buffer, size_t len) const{
    ssize_t res = send(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw socketException("Cannot write to socket");
    return res;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len) const{
    if(::connect(socket_fd, reinterpret_cast<struct sockaddr*>(addr), len) != 0)
        throw socketException("Cannot connect to remote socket");
}

void Socket::connectToServer(std::string address, int port) {
    //todo: controllare che addr non venga distrutto
    struct sockaddr_in addr;
    inet_pton(AF_INET, address.c_str(), &(addr.sin_addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    this->connect(&addr, sizeof(addr));
}

void Socket::closeConnection() const {
    ::close(socket_fd);
}

int Socket::Select(int max_fd, fd_set *read_set, fd_set *write_set, fd_set *except_set, struct timeval *timeout){
    int n;
    again:
    if ( (n = select (max_fd, read_set, write_set, except_set, timeout)) < 0)
    {
        if (INTERRUPTED_BY_SIGNAL)
            goto again;
        else{
            perror("Select error");
            return n;
        }
    }
    return n;
}

void Socket::sendJSON(const std::string &JSON) {
    this->sendString(JSON);
}

void Socket::sendString(const std::string& str){
    fd_set write_fs;
    ssize_t size_left = str.size();
    const char* ptr = str.c_str();
    struct timeval timeout;
    int k;
    this->setSelect(write_fs, timeout);

    while (size_left>0){
        timeout.tv_usec = 0;
        timeout.tv_sec = Socket::timeout_value;
        if( (k = this->Select(FD_SETSIZE, nullptr, &write_fs, nullptr, &timeout))<0){
            std::cout << "Errore select" << std::endl;
            throw socketException("Select error");
        }
        if(k<1 || !FD_ISSET(this->socket_fd, &write_fs)){
            std::cout << "Timeout scaduto" << std::endl;
            throw socketException("Elapsed timeout");
        }
        std::cout << "Invio str" << std::endl;

        ssize_t size_written = this->write(ptr, size_left);
        if (size_written <= 0){
            if (INTERRUPTED_BY_SIGNAL){
                size_written = 0;
                continue; /* and call send() again */
            }
            else
                throw socketException("Remote socket closed");
        }
        size_left -= size_written;
        ptr += size_written;
    }
}

void Socket::sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile) {
    char buffer[N];
    fd_set write_fs;
    ssize_t size_read = 0;
    struct timeval timeout;
    int k;
    const bool isFile = syncedFile->isFile() && syncedFile->getFileStatus()!=FileStatus::erased;
//    syncedFile->update_file_data();
    std::ifstream file_to_send(syncedFile->getPath(), std::ios::binary);



    this->setSelect(write_fs, timeout);
    std::cout << "isFile: " << isFile << std::endl;

    if(isFile) {
        // todo: se apro il file l'os dovrebbe impedire la modifica agli altri programmi?
        // todo: se il file non esiste più cosa faccio? Lo marchio come eliminato o ignoro il problema visto che alla prossima scansione si accorgerà che il file è stato eliminato?
//        file_to_send = std::ifstream(syncedFile->getPath(), std::ios::binary);
        // se il file non esiste ignoro il problema ed esco, alla prossima scansione del file system verrà notata la sua assenza
        std::cout << "Invio il file" << std::endl;
        // leggo e invio il file
        std::cout << "Size to read: " << syncedFile->getFileSize() << std::endl;
        unsigned long size = 1;
        // ciclo finchè non ho letto tutto il file
        while (size_read < syncedFile->getFileSize() && size>0) {
            file_to_send.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            size = file_to_send.gcount();
            ssize_t size_left = size;
            char* ptr = buffer;

            // ciclo per inviare tutto il buffer
            while (size_left>0){
                timeout.tv_usec = 0;
                timeout.tv_sec = Socket::timeout_value;
                if( (k = this->Select(FD_SETSIZE, nullptr, &write_fs, nullptr, &timeout))<0){
                    std::cout << "Errore select" << std::endl;
                    throw socketException("Select error");
                }
                if(k<1 || !FD_ISSET(this->socket_fd, &write_fs)){
                    std::cout << "Timeout scaduto" << std::endl;
                    throw socketException("Elapsed timeout");
                }
                std::cout << "Invio file" << std::endl;

                ssize_t size_written = this->write(ptr, size_left);
                if (size_written <= 0){
                    if (INTERRUPTED_BY_SIGNAL){
                        size_written = 0;
                        continue; /* and call send() again */
                    }
                    else
                        throw socketException("Remote socket closed");
                }
                size_left -= size_written;
                ptr += size_written;
            }
            size_read += size;
            std::cout << "Size read: " << size_read << std::endl;
        }
        std::cout << "File chiuso" << std::endl;
        file_to_send.close();
    }
}

std::string Socket::getJSON() {
    fd_set read_fds;
    bool continue_cycle = true;
    ssize_t l = 0;
    int k;
    char buffer[N];
    struct timeval timeout;
    this->setSelect(read_fds, timeout);

    while (l<N-1 && continue_cycle){
        timeout.tv_usec = 0;
        timeout.tv_sec = Socket::timeout_value;
        if( (k = this->Select(FD_SETSIZE, &read_fds, nullptr, nullptr, &timeout))<0){
            std::cout << "Errore select" << std::endl;
            throw socketException("Select error");
        }
        if(k<1 || !FD_ISSET(this->socket_fd, &read_fds)){
            std::cout << "Timeout scaduto" << std::endl;
            throw socketException("Elapsed timeout");
        }
        std::cout << "Ricevo json" << std::endl;
        ssize_t size_read = this->read(buffer+l, N-1-l);

        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0)
            throw socketException("Remote socket closed");
        l += size_read;

        // todo: cambiare metodo di terminzione un file può contenere il carattere } quindi non funzionerebbe
        for(int i=0; i<l; i++){
            if (buffer[i] == '}')
                continue_cycle = false;
        }
    }
    buffer[l] = '\0';
    return buffer;
}

void Socket::setSelect(fd_set& cset, struct timeval& timeout){
    timeout.tv_sec = Socket::timeout_value;
    timeout.tv_usec = 0;
    FD_ZERO(&cset);
    FD_SET(this->socket_fd, &cset);
}

// genero un nome temporaneo per il file dato da tempo corrente + id thread
// todo: verificare che lo sha256 non sia diverso per il nome del file
std::string Socket::tempFilePath(){
    auto th_id = std::this_thread::get_id();
    std::time_t t = std::time(nullptr);   // get time now
    std::stringstream ss;
    ss << "temp/" << th_id << "-" << t;
    return ss.str();
}

std::string Socket::getFile(unsigned long size) {
    fd_set read_fds;
    ssize_t l = 0;
    char buffer[N];
    struct timeval timeout;
    int k;

    // todo: trovare un nome per nominare i file provvisori
    std::string file_path(tempFilePath());

    // apro il file
    // todo: create cartella temp se non esiste
    std::ofstream file(file_path, std::ios::binary);
    // se non riesco ad aprire il file ritorno nullopt
    if(!file.is_open()){
        std::cout << "File not opened: " << file_path << std::endl;
        throw filesystemException("File not opened");
    }

    this->setSelect(read_fds, timeout);

    while (l<size){
        timeout.tv_usec = 0;
        timeout.tv_sec = Socket::timeout_value;
        if( (k = this->Select(FD_SETSIZE, &read_fds, nullptr, nullptr, &timeout))<0){
            std::cout << "Errore select" << std::endl;
            throw socketException("Select error");
        }
        if(k<1 || !FD_ISSET(this->socket_fd, &read_fds)){
            std::cout << "Timeout scaduto" << std::endl;
            throw socketException("Elapsed timeout");
        }
        ssize_t size_read = this->read(buffer, N);
        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0){
            std::cout << "Remote socket closed" << std::endl;
            throw socketException("Remote socket closed");
        }
        l += size_read;
        file.write(buffer, size_read);
    }
    return file_path;
}

void Socket::fileError() {

}

const std::string &Socket::getUsername() const {
    return username;
}

void Socket::setUsername(std::string u) {
    this->username = std::move(u);
}

void Socket::sendKOResp() {
    sendString("KO");
}

void Socket::sendOKResp() {
    sendString("OK");
}

void Socket::sendNOResp() {
    sendString("NO");
}

// attendo la ricezione di uno stato tra {OK, NO, KO}
std::string Socket::getResp() {
    fd_set read_fds;
    ssize_t l = 0;
    char buffer[N];
    struct timeval timeout;
    int k;
    this->setSelect(read_fds, timeout);

    while (l<2){
        timeout.tv_usec = 0;
        timeout.tv_sec = Socket::timeout_value;
        if( (k = this->Select(FD_SETSIZE, &read_fds, nullptr, nullptr, &timeout))<0){
            std::cout << "Errore select" << std::endl;
            throw socketException("Select error");
        }
        if(k<1 || !FD_ISSET(this->socket_fd, &read_fds)){
            std::cout << "Timeout scaduto" << std::endl;
            throw socketException("Elapsed timeout");
        }
        ssize_t size_read = this->read(buffer, N);
        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0){
            std::cout << "Remote socket closed" << std::endl;
            throw socketException("Remote socket closed");
        }
        l += size_read;
    }
    buffer[2] = '\0';
    std::string value(buffer);
    std::cout << value << std::endl;
    // controllo se il valore ricevuto è tra quelli ammissibili, se non lo è ritorno un nullpt
    if(value != "OK" && value != "NO" && value != "KO"){
        std::cout << "Not valid resp" << std::endl;
        throw dataException("Not valid response");
    }
    return value;
}

void Socket::clearReadBuffer(){
    char buffer[N];
    while (sockReadIsReady()){
        ssize_t res = recv(socket_fd, buffer, N, MSG_NOSIGNAL);
        buffer[res] = '\0';
        std::cout << "CLEAR:$" << buffer << "$"<< std::endl;
        if(res<=0)
            return;
    }
}

// se c'è qualcosa da leggere nella socket la FD_ISSET ritornerà true
bool Socket::sockReadIsReady() {
    fd_set read_fds;
    struct timeval timeout;
    int k;
    this->setSelect(read_fds, timeout);
    timeout.tv_usec = 0;
    timeout.tv_sec = 0;
    if( (k = this->Select(FD_SETSIZE, &read_fds, nullptr, nullptr, &timeout))<0){
        std::cout << "Errore select" << std::endl;
        throw socketException("Select error");
    }
    if(k<1 || !FD_ISSET(this->socket_fd, &read_fds)){
        return false;
    }
    std::cout << "Sock is ready" << std::endl;
    return true;
}



