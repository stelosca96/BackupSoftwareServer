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

static const int max_thread = 2;

// todo: gestire eccezioni errori
// todo: file config (max_threads, port)
// todo: timeouts
int main() {

    boost::asio::io_service io_service;

    Server server(io_service, 9999);
    server.do_accept();
    std::vector<std::thread> threads;
    threads.reserve(max_thread-1);
    for(int i=0; i<max_thread-1; i++)
        threads.emplace_back(std::thread(
                [&io_service](){
                    io_service.run();
                }));
    io_service.run();
}
