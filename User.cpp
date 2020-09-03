//
// Created by stefano on 12/08/20.
//

#include "User.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <utility>
#include <openssl/sha.h>
#include <ctime>

namespace pt = boost::property_tree;

User::User(const std::string& JSON) {
    std::stringstream ss(JSON);
    boost::property_tree::ptree root;
    boost::property_tree::read_json(ss, root);
    this->username = root.get_child("username").data();
    this->password = root.get_child("password").data();
}

std::string User::randomString(size_t length){
    auto randChar = []() -> char{
        const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ random() % max_index ];
    };
    unsigned seed = time(nullptr);
    srandom(seed);
    std::string str(length,0);
    std::generate_n( str.begin(), length, randChar );
    return str;
}

std::string User::getHash(const std::string& password){
    unsigned char c_hash[SHA256_DIGEST_LENGTH];
    std::string str = this->salt + password;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(c_hash, &sha256);
    std::stringstream ss;
    for(unsigned char i : c_hash)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)i;
    }
    return ss.str();
};

const std::string &User::getUsername() const {
    return username;
}

User::User():username(""), salt(""), hash("") {}

const std::string &User::getSalt() const {
    return salt;
}

const std::string &User::getHashedPassword() const {
    return hash;
}

User::User(std::string username, std::string hashPassword, std::string salt): username(std::move(username)), salt(std::move(salt)), hash(std::move(hashPassword)) {
}

void User::setSalt(const std::string &salt) {
    this->salt = salt;
    this->hash = this->getHash(this->password);
}

User::User(const User &user): username(user.username), password(user.password), salt(user.password), hash(user.hash) {}

void User::calculateHash() {
    this->salt = randomString(N);
    this->hash = this->getHash(this->password);
}

