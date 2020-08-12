//
// Created by stefano on 13/06/20.
//

#ifndef PDS_LAB5_SERVERSOCKET_H
#define PDS_LAB5_SERVERSOCKET_H


#include <cstdint>
#include "Socket.h"
#include <sys/socket.h>

class ServerSocket {
private:
    int server_sock;
public:

    ServerSocket(uint16_t port_number, int backlog);
    ~ServerSocket();


    Socket accept(sockaddr *pSockaddr, socklen_t *pInt);
};


#endif //PDS_LAB5_SERVERSOCKET_H
