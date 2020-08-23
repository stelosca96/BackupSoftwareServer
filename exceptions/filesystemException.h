//
// Created by stefano on 23/08/20.
//

#ifndef SERVERBACKUP_FILESYSTEMEXCEPTION_H
#define SERVERBACKUP_FILESYSTEMEXCEPTION_H


#include <stdexcept>

class filesystemException: public std::runtime_error{
public:
    explicit filesystemException(const std::string &arg) : runtime_error(arg){}
};


#endif //SERVERBACKUP_FILESYSTEMEXCEPTION_H
