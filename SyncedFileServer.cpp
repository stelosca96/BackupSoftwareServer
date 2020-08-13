//
// Created by stefano on 06/08/20.
//
#include <openssl/sha.h>
#include <fstream>
#include <iomanip>
#include <utility>
#include "SyncedFileServer.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <iostream>

namespace pt = boost::property_tree;

static const int K_READ_BUF_SIZE{ 1024 * 16 };

SyncedFileServer::SyncedFileServer(const std::string& JSON){
    // todo: gestire eccezioni
    std::stringstream ss(JSON);

    boost::property_tree::ptree root;
    boost::property_tree::read_json(ss, root);

    this->hash = root.get_child("hash").data();

    // If no conversion could be performed, an invalid_argument exception is thrown.
    this->file_size = std::stoul(root.get_child("file_size").data());

    // If no conversion could be performed, an invalid_argument exception is thrown.
    this->fileStatus = static_cast<FileStatus>(std::stoi(root.get_child("file_status").data()));

    // If no conversion could be performed, an invalid_argument exception is thrown.
    this->is_file = std::stoi(root.get_child("is_file").data());
}

void SyncedFileServer::update_file_data() {
    std::string new_hash = CalcSha256(path);
    if(new_hash!= this->hash){
        this->is_file = std::filesystem::is_regular_file(this->path);
        this->fileStatus = FileStatus::modified;
        this->hash = new_hash;
        this->file_size = is_file ? std::filesystem::file_size(this->path) : 0;
    }
}


std::string SyncedFileServer::CalcSha256(const std::string& filename){
    // Initialize openssl
    SHA256_CTX context;
    if(!SHA256_Init(&context))
        return "";

    // Read file and update calculated SHA
    char buf[K_READ_BUF_SIZE];
    std::ifstream file(filename, std::ifstream::binary);
    while (file.good()){
        file.read(buf, sizeof(buf));
        if(!SHA256_Update(&context, buf, file.gcount()))
            return "";
    }

    // Get Final SHA
    unsigned char result[SHA256_DIGEST_LENGTH];
    if(!SHA256_Final(result, &context))
        return "";

    // Transform byte-array to string
    std::stringstream sha_str;
    sha_str << std::hex << std::setfill('0');
    for (const auto &byte: result)
        sha_str << std::setw(2) << (int)byte;
    return sha_str.str();
}

bool SyncedFileServer::operator==(const SyncedFileServer &rhs) const {
    return path == rhs.path &&
           hash == rhs.hash;
}

bool SyncedFileServer::operator!=(const SyncedFileServer &rhs) const {
    return !(rhs == *this);
}

SyncedFileServer::SyncedFileServer(SyncedFileServer const &syncedFile) {
    this->fileStatus = syncedFile.fileStatus;
    this->hash = syncedFile.hash;
    this->path = syncedFile.path;
    this->is_file = syncedFile.is_file;
}

// todo: da rimuovere, ma se lo togli non funziona nulla
SyncedFileServer::SyncedFileServer() {
    fileStatus = FileStatus::not_valid;
    is_file = false;
}

const std::string &SyncedFileServer::getPath() const {
    return path;
}

const std::string &SyncedFileServer::getHash() const {
    return hash;
}

std::string SyncedFileServer::to_string() {
    return this->path + " (" + this->hash + ")";
}

FileStatus SyncedFileServer::getFileStatus() const {
    return fileStatus;
}

bool SyncedFileServer::isSyncing() const {
    return is_syncing;
}

void SyncedFileServer::setSyncing(bool syncing) {
    SyncedFileServer::is_syncing = syncing;
}

std::string SyncedFileServer::getJSON() {
    pt::ptree root;
    root.put("path", this->path);

    //todo: devo ricalcolare l'hash e la dimensione (nel caso il file sia stato modificato) o uso quello salvato?
    // this->update_file_data();
    root.put("hash", this->hash);
    root.put("file_size", this->file_size);
    root.put("file_status", static_cast<int>(this->fileStatus));
    root.put("is_file", this->is_file);

    //todo: nel json posso aggiungere anche informazioni per l'autenticazione

    std::stringstream ss;
    pt::json_parser::write_json(ss, root);
    std::cout << ss.str() << std::endl;
    return ss.str();
}

bool SyncedFileServer::isFile() const {
    return is_file;
}

unsigned long SyncedFileServer::getFileSize() const {
    return file_size;
}



