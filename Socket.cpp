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

Socket::Socket(Socket &&other) noexcept : socket_fd(other.socket_fd){  // costruttore di movimento
    other.socket_fd = 0;
}

Socket& Socket::operator=(Socket &&other) noexcept {   // costruttore di copia per movimento
    if(socket_fd != 0) close(socket_fd);
    socket_fd = other.socket_fd;
    other.socket_fd = 0;
    return *this;
}

ssize_t Socket::read(char *buffer, size_t len) const{
    ssize_t res = recv(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw std::runtime_error("Cannot receive from socket");
    return res;
}

ssize_t Socket::write(const char *buffer, size_t len) const{
    ssize_t res = send(socket_fd, buffer, len, MSG_NOSIGNAL);
    if(res < 0) throw std::runtime_error("Cannot write to socket");
    return res;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len) const{
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

void Socket::closeConnection() const {
    ::close(socket_fd);
}

bool Socket::sendJSON(const std::string& JSON){
    fd_set write_fs;
    if(!setWriteSelect(write_fs))
        return false;
    ssize_t size_write = this->write(JSON.c_str(), JSON.size());

    // se la dimensione letta è zero la socket è stata chiusa
    return (size_write==JSON.size());
}

bool Socket::sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile) {
    char buffer[N];
    fd_set write_fs;
    unsigned long size_read = 0;
    const bool isFile = syncedFile->isFile() && syncedFile->getFileStatus()!=FileStatus::erased;
//    syncedFile->update_file_data();
    std::ifstream file_to_send(syncedFile->getPath(), std::ios::binary);
    if(isFile) {
        // todo: se apro il file l'os dovrebbe impedire la modifica agli altri programmi?
        // todo: se il file non esiste più cosa faccio? Lo marchio come eliminato o ignoro il problema visto che alla prossima scansione si accorgerà che il file è stato eliminato?
//        file_to_send = std::ifstream(syncedFile->getPath(), std::ios::binary);
        // se il file non esiste ignoro il problema ed esco, alla prossima scansione del file system verrà notata la sua assenza
        if (!file_to_send.is_open())
            return false;
        std::cout << "Invio il file" << std::endl;
        // leggo e invio il file
        std::cout << "Size to read: " << file_to_send.tellg() << std::endl;
        unsigned long size = 1;
        while (size_read < syncedFile->getFileSize() && size>0) {
            if(!setWriteSelect(write_fs))
                return false;
            file_to_send.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            size = file_to_send.gcount();
            ssize_t s = this->write(buffer, size);
            if(size!=s){
                // todo: fare qualcosa per gestire il problema
            }
            size_read += s;
            std::cout << "Size read: " << size_read << std::endl;
        }
        file_to_send.close();
    }
    return true;
}

std::string Socket::readJSON() {
    fd_set read_fds;
    bool continue_cycle = true;
    ssize_t l = 0;
    char buffer[N];
    std::cout << "Read JSON" << std::endl;

    while (l<N-1 && continue_cycle){
        std::cout << "WHILE" << std::endl;
        if(!setReadSelect(read_fds)){
            std::cout << "Select ERROR" << std::endl;
            throw std::runtime_error("Timeout expired");
        }
        std::cout << "dopo WHILE" << std::endl;
        ssize_t size_read = this->read(buffer+l, N-1-l);
        std::cout << buffer << std::endl;
        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0)
            throw std::runtime_error("Socket is closed");
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

bool Socket::setReadSelect(fd_set &read_fds){
    // todo: la select non funziona e non so il perchè

    std::cout << "Qua 0" << std::endl;

    FD_ZERO(&read_fds);
    if(this->socket_fd < 0)
        return false;
    std::cout << "Qua 1" << std::endl;

    FD_SET(this->socket_fd, &read_fds);
    struct timeval timeout;
    std::cout << "Qua 2" << std::endl;
    timeout.tv_sec = Socket::timeout_value;
    timeout.tv_usec = 0;
    std::cout << "Qua 3" << std::endl;

    if(select(this->socket_fd, &read_fds, nullptr, nullptr, &timeout)<=0){
        // ritorno false sia nel caso che il tiemeout scada, sia nel caso in cui la select non vada a buon fine
        std::cout << "Qua 4" << std::endl;
        // select error: ritorno una stringa vuota così fallirà il checksum, oppure lancio un'eccezione?
        return false;
    }
    std::cout << "Qua 5" << std::endl;
//    return true;
    return (FD_ISSET(this->socket_fd, &read_fds));
}

bool Socket::setWriteSelect(fd_set &write_fds){
    FD_ZERO(&write_fds);
    if(this->socket_fd < 0)
        return false;
    FD_SET(this->socket_fd, &write_fds);
    struct timeval timeout;
    timeout.tv_sec = Socket::timeout_value;
    timeout.tv_usec = 0;
    if(select(this->socket_fd, nullptr, &write_fds, nullptr, &timeout)<=0)
        // select error: ritorno una stringa vuota così fallirà il checksum, oppure lancio un'eccezione?
        return false;
    return true;
    // todo: la fd_isset non funziona e non so il perchè
//    return (FD_ISSET(this->socket_fd, &write_fds));
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

std::optional<std::string> Socket::getFile(unsigned long size) {
    fd_set read_fds;
    ssize_t l = 0;
    char buffer[N];
    // todo: trovare un nome per nominare i file provvisori
    std::string file_path(tempFilePath());

    // apro il file
    std::ofstream file(file_path, std::ios::binary);
    // se non riesco ad aprire il file ritorno nullopt
    if(!file.is_open())
        return std::nullopt;

    while (l<size){
        if(!setReadSelect(read_fds))
            return std::nullopt;
        ssize_t size_read = this->read(buffer, N);
        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0)
            return std::nullopt;
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

bool Socket::sendKOResp() {
    return sendResp("KO");
}

bool Socket::sendOKResp() {
    return sendResp("OK");
}

bool Socket::sendNOResp() {
    return sendResp("NO");
}

// attendo la ricezione di uno stato tra {OK, NO, KO}
std::optional<std::string> Socket::getResp() {
    fd_set read_fds;
    ssize_t l = 0;
    char buffer[N];

    while (l<2){
        if(!setReadSelect(read_fds))
            return std::nullopt;
        ssize_t size_read = this->read(buffer, N);
        // se la dimensione letta è zero la socket è stata chiusa
        if(size_read==0)
            return std::nullopt;
        l += size_read;
    }
    buffer[3] = '\0';
    std::string value(buffer);

    // controllo se il valore ricevuto è tra quelli ammissibili, se non lo è ritorno un nullpt
    if(value != "OK" && value != "NO" && value != "KO")
        return std::nullopt;
    return value;
}

// ritorno true in caso che l'invio avvenga con successo
bool Socket::sendResp(std::string resp) {
    fd_set write_fs;
    ssize_t l = 0;

    if(!setWriteSelect(write_fs))
        return false;
    ssize_t size_write = this->write(resp.c_str(), resp.size());

    // se la dimensione letta è zero la socket è stata chiusa
    return (size_write!=0);
}

// se c'è qualcosa da leggere nella socket la FD_ISSET ritornerà true
bool Socket::sockReadIsReady() {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    if(this->socket_fd < 0)
        return "";
    FD_SET(this->socket_fd, &read_fds);
    struct timeval timeout;
    timeout.tv_sec = 0;
    if(select(this->socket_fd, &read_fds, nullptr, nullptr, &timeout)<0)
        // select error: ritorno una stringa vuota così fallirà il checksum, oppure lancio un'eccezione?
        throw std::runtime_error("Select error");
    return (FD_ISSET(this->socket_fd, &read_fds));}



