//
// Created by stefano on 12/08/20.
//

#ifndef SERVERBACKUP_USER_H
#define SERVERBACKUP_USER_H


#include <string>

class User {
private:
    std::string username;
    std::string password;
public:
    User(const std::string& JSON);
    User(const std::string &username, const std::string &password);

    const std::string &getUsername() const;

    const std::string &getPassword() const;

    std::string getJSON();
};


#endif //SERVERBACKUP_USER_H
