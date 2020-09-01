//
// Created by stefano on 12/08/20.
//

#ifndef SERVERBACKUP_USER_H
#define SERVERBACKUP_USER_H


#include <string>

class User {
private:
    std::string username;
    std::string salt;
    std::string password;
    std::string hash;
    const static unsigned int N = 7;

    std::string getHash(const std::string &password);
    static std::string randomString(size_t length);
public:
    User();

    explicit User(const std::string& JSON);
    User(std::string username, std::string hashPassword, std::string salt);
    User(const User &user);

    [[nodiscard]] const std::string &getUsername() const;
    [[nodiscard]] const std::string &getHashedPassword() const;
    [[nodiscard]] const std::string &getSalt() const;

    void calculateHash();
    void setSalt(const std::string &salt);
};


#endif //SERVERBACKUP_USER_H
