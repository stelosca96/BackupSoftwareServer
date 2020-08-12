//
// Created by stefano on 13/06/20.
//

#include <netinet/in.h>
#include <stdexcept>
#include <cstring>
#include "ServerSocket.h"
#include "Socket.h"
#include <unistd.h>
#include <iostream>

ServerSocket::ServerSocket(uint16_t port_number, int backlog) {
    struct sockaddr_in addr;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock == -1)
        throw std::runtime_error(strerror(errno));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_number);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if((bind(server_sock, (struct sockaddr*)&addr, sizeof(addr))) == -1)
        throw std::runtime_error(strerror(errno));
    if(listen(server_sock, backlog) == -1)
        throw std::runtime_error(strerror(errno));

}

ServerSocket::~ServerSocket() {
    std::cout << "Closing socket server" << std::endl;
    ::close(server_sock);

}

Socket ServerSocket::accept(sockaddr *addr, socklen_t *len) {
    int accept = ::accept(server_sock, reinterpret_cast<struct sockaddr*>(&addr), len);
    if (accept < 0){
        std::cout << "Accept -1" << std::endl;
        throw std::runtime_error(strerror(errno));
    }
    std::cout << "accept: "  << accept << std::endl;
    return Socket(accept);
}

