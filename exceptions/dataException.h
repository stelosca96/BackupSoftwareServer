//
// Created by stefano on 23/08/20.
//

#ifndef SERVERBACKUP_DATAEXCEPTION_H
#define SERVERBACKUP_DATAEXCEPTION_H


#include <stdexcept>

class dataException : public std::runtime_error{
public:
    explicit dataException(const std::string &arg) : runtime_error(arg) {}
};


#endif //SERVERBACKUP_DATAEXCEPTION_H
