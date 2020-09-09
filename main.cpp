#include <iostream>
#include <unordered_map>
#include <thread>
#include "SyncedFileServer.h"
#include "User.h"
#include "exceptions/socketException.h"
#include "exceptions/dataException.h"
#include "exceptions/filesystemException.h"
#include "Server.h"
#include <filesystem>
#include <fstream>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <shared_mutex>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <boost/asio.hpp>


namespace pt = boost::property_tree;
using boost::asio::ip::tcp;
using boost::asio::deadline_timer;

// todo: gestire eccezioni errori
// todo: file config (max_threads, port)
int main() {
    std::string crt, key, dhTemp, crtPsw;
    int max_threads, port;
    try {
        pt::ptree root;
        pt::read_json("config.json", root);
        max_threads = std::stoi(root.get_child("max_threads").data());
        port = std::stoi(root.get_child("port").data());
        crt = root.get_child("cert_path").data();
        key = root.get_child("cert_key").data();
        dhTemp = root.get_child("dh_pem").data();
        crtPsw = root.get_child("cert_password").data();
        if(port<0 || max_threads<0)
            throw std::runtime_error("Port o max_threads non possono essere minori di 0");
    } catch (std::runtime_error &error) {
        std::cerr << "Errore caricamento file configurazione" << std::endl;
        std::cerr << error.what() << std::endl;
        exit(-1);
    }
    boost::asio::io_service io_service;
    Server server(io_service, port, crt, key, crtPsw, dhTemp);
    std::cout << "Il server si sta avviando sulla porta: " << port << std::endl;
    server.do_accept();
    std::vector<std::thread> threads;
    threads.reserve(max_threads-1);
    for(int i=0; i<max_threads-1; i++)
        threads.emplace_back(std::thread(
                [&io_service](){
                    io_service.run();
                }));
    io_service.run();
}
