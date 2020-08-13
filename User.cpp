//
// Created by stefano on 12/08/20.
//

#include "User.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>

namespace pt = boost::property_tree;

User::User(const std::string &username, const std::string &password) : username(username), password(password) {}

User::User(std::string JSON) {
    // todo: gestire eccezioni
    std::stringstream ss(JSON);
    boost::property_tree::ptree root;
    boost::property_tree::read_json(ss, root);
    this->username = root.get_child("username").data();
    this->password = root.get_child("password").data();
}

std::string User::getJSON() {
    pt::ptree root;
    root.put("username", this->username);
    root.put("password", this->password);

    std::stringstream ss;
    pt::json_parser::write_json(ss, root);
    std::cout << ss.str() << std::endl;
    return ss.str();}

const std::string &User::getUsername() const {
    return username;
}

const std::string &User::getPassword() const {
    return password;
}

User::User(const User &user): username(user.username), password(user.password)  {
}

User::User():username(""), password("") {}

