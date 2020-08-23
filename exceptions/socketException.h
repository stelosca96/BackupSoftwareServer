//
// Created by stefano on 23/08/20.
//

#ifndef SERVERBACKUP_SOCKETEXCEPTION_H
#define SERVERBACKUP_SOCKETEXCEPTION_H


#include <stdexcept>

class socketException: public std::runtime_error{
public:
    explicit socketException(const std::string &arg) : runtime_error(arg){}
};


#endif //SERVERBACKUP_SOCKETEXCEPTION_H
