////
//// Created by stefano on 07/08/20.
////
//
//#include "Socket.h"
//#include "exceptions/socketException.h"
//#include "exceptions/dataException.h"
//#include "exceptions/filesystemException.h"
//#include <iostream>
//#include <unistd.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <bits/stdc++.h>
//#include <fstream>
//#include <utility>
//#include <boost/lambda/lambda.hpp>
//
//using boost::lambda::var;
//
//// todo: valutare se utile https://www.codeproject.com/Articles/1264257/Socket-Programming-in-Cplusplus-using-boost-asio-T
//
//Socket::Socket(tcp::socket& sock1, deadline_timer& timer) : sock(std::move(sock1)), timeout(std::move(timer)) {
//    timeout.expires_at(boost::posix_time::pos_infin);
//}
//
//Socket::~Socket() {
//    this->sock.close();
//}
//
//void Socket::connectToServer(std::string address, int port) {
////    //todo: controllare che addr non venga distrutto
////    struct sockaddr_in addr;
////    inet_pton(AF_INET, address.c_str(), &(addr.sin_addr));
////    addr.sin_port = htons(port);
////    addr.sin_family = AF_INET;
////    this->connect(&addr, sizeof(addr));
//}
//
//void Socket::closeConnection(){
//    this->sock.close();
//}
//
//void Socket::sendJSON(const std::string &JSON) {
//    this->sendString(JSON);
//}
//
//void Socket::sendString(const std::string& str){
//    // todo: boost::system::system_error	Thrown on failure.
//    // todo: il file è inviato tutto?
//    // todo: timeout
//    std::string buffer = str + "\\\n";
//    boost::asio::write(this->sock, boost::asio::buffer(buffer, buffer.size()));
//}
//
//std::string Socket::readString(){
//    // todo: boost::system::system_error	Thrown on failure.
//    // todo: timeout
//    boost::asio::streambuf buf;
//
//
//    // Set a deadline for the asynchronous operation. Since this function uses
//    // a composed operation (async_read_until), the deadline applies to the
//    // entire operation, rather than individual reads from the socket.
//    unsigned int t = this->timeout_value;
//    this->timeout.expires_from_now(boost::posix_time::seconds(t));
//
//
//    // Set up the variable that receives the result of the asynchronous
//    // operation. The error code is set to would_block to signal that the
//    // operation is incomplete. Asio guarantees that its asynchronous
//    // operations will never fail with would_block, so any other value in
//    // ec indicates completion.
//    boost::system::error_code ec = boost::asio::error::would_block;
//
//
//    // Start the asynchronous operation itself. The boost::lambda function
//    // object is used as a callback and will update the ec variable when the
//    // operation completes.
//    // uso il terminatore \ a capo per la ricezione dopo averlo aggiunto nell'invio
//    boost::asio::read_until(this->sock,buf,"\\\n");
//
////    // Block until the asynchronous operation has completed.
////    do io_service_.run_one(); while (ec == boost::asio::error::would_block);
////
////    if (ec)
////        throw boost::system::system_error(ec);
//
//    std::string data = boost::asio::buffer_cast<const char*>(buf.data());
//    // rimuovo i terminatori quindi gli ultimi due caratteri
//    return data.substr(0, data.length()-2);
//}
//
//void Socket::sendFile(const std::shared_ptr<SyncedFileServer>& syncedFile) {
//    char buffer[N];
//    fd_set write_fs;
//    ssize_t size_read = 0;
//    struct timeval timeout;
//    int k;
//    const bool isFile = syncedFile->isFile() && syncedFile->getFileStatus()!=FileStatus::erased;
//    std::ifstream file_to_send(syncedFile->getPath(), std::ios::binary);
//
//    std::cout << "isFile: " << isFile << std::endl;
//
//    if(isFile) {
//        // todo: se apro il file l'os dovrebbe impedire la modifica agli altri programmi?
//        // todo: se il file non esiste più cosa faccio? Lo marchio come eliminato o ignoro il problema visto che alla prossima scansione si accorgerà che il file è stato eliminato?
////        file_to_send = std::ifstream(syncedFile->getPath(), std::ios::binary);
//        // se il file non esiste ignoro il problema ed esco, alla prossima scansione del file system verrà notata la sua assenza
//        std::cout << "Invio il file" << std::endl;
//        // leggo e invio il file
//        std::cout << "Size to read: " << syncedFile->getFileSize() << std::endl;
//        unsigned long size = 1;
//        // ciclo finchè non ho letto tutto il file
//        while (size_read < syncedFile->getFileSize() && size>0) {
//            file_to_send.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
//            size = file_to_send.gcount();
//            // todo: boost::system::system_error	Thrown on failure.
//            // todo: timeout
//            boost::asio::write(this->sock, boost::asio::buffer(buffer, size));
//            size_read += size;
//            std::cout << "Size read: " << size_read << std::endl;
//        }
//        std::cout << "File chiuso" << std::endl;
//        file_to_send.close();
//    }
//}
//
//std::string Socket::getJSON() {
//    return this->readString();
//}
//
//// genero un nome temporaneo per il file dato da tempo corrente + id thread
//std::string Socket::tempFilePath(){
//    std::filesystem::create_directory("temp/");
//    auto th_id = std::this_thread::get_id();
//    std::time_t t = std::time(nullptr);   // get time now
//    std::stringstream ss;
//    ss << "temp/" << th_id << "-" << t;
//    return ss.str();
//}
//
//std::string Socket::getFile(unsigned long size) {
//    ssize_t l = 0;
//
//    // creo un nome file provvisorio univoco tra tutti i thread
//    std::string file_path(tempFilePath());
//
//    // apro il file
//    std::ofstream file(file_path, std::ios::binary);
//
//    // se non riesco ad aprire il file lancio un'eccezione
//    if(!file.is_open()){
//        std::cout << "File not opened: " << file_path << std::endl;
//        throw filesystemException("File not opened");
//    }
//
//    while (l<size){
//        // todo: fixare lettura di massimo N byte
//        boost::asio::streambuf buf(N);
//        ssize_t size_read = boost::asio::read(this->sock, buf);
//
//        // se la dimensione letta è zero la socket è stata chiusa
//        if(size_read==0){
//            std::cout << "Remote socket closed" << std::endl;
//            throw socketException("Remote socket closed");
//        }
//        l += size_read;
//        // faccio un cast per scrivere sul file
//        std::string data = boost::asio::buffer_cast<const char*>(buf.data());
//        file.write(data.c_str(), size_read);
//    }
//    return file_path;
//}
//
//const std::string &Socket::getUsername() const {
//    return username;
//}
//
//void Socket::setUsername(std::string u) {
//    this->username = std::move(u);
//}
//
//void Socket::sendKOResp() {
//    sendString("KO");
//}
//
//void Socket::sendOKResp() {
//    sendString("OK");
//}
//
//void Socket::sendNOResp() {
//    sendString("NO");
//}
//
//// attendo la ricezione di uno stato tra {OK, NO, KO}
//std::string Socket::getResp() {
//    std::string value = this->readString();
//    std::cout << value << std::endl;
//    // controllo se il valore ricevuto è tra quelli ammissibili, se non lo è ritorno un'eccezione
//    if(value != "OK" && value != "NO" && value != "KO"){
//        std::cout << "Not valid resp" << std::endl;
//        throw dataException("Not valid response");
//    }
//    return value;
//}
//
//void Socket::clearReadBuffer(){
//    // todo: da implementare
////    char buffer[N];
////    while (sockReadIsReady()){
////        ssize_t res = recv(socket_fd, buffer, N, MSG_NOSIGNAL);
////        buffer[res] = '\0';
////        std::cout << "CLEAR:$" << buffer << "$"<< std::endl;
////        if(res<=0)
////            return;
////    }
//}
//
//// se c'è qualcosa da leggere nella socket la FD_ISSET ritornerà true
//bool Socket::sockReadIsReady() {
//    // todo: da implementare
//
////    fd_set read_fds;
////    struct timeval timeout;
////    int k;
////    this->setSelect(read_fds, timeout);
////    timeout.tv_usec = 0;
////    timeout.tv_sec = 0;
////    if( (k = this->Select(FD_SETSIZE, &read_fds, nullptr, nullptr, &timeout))<0){
////        std::cout << "Errore select" << std::endl;
////        throw socketException("Select error");
////    }
////    if(k<1 || !FD_ISSET(this->socket_fd, &read_fds)){
////        return false;
////    }
////    std::cout << "Sock is ready" << std::endl;
//    return true;
//}
//
//Socket &Socket::operator=(Socket &&other) {
//    // todo: verificare che non cei eccezioni
//    this->sock.close();
//    this->sock = std::move(other.sock);
//    this->username = other.username;
//    return *this;
//}
//
//Socket::Socket(Socket &&other): sock(std::move(other.sock)), username(other.username){
//}
//
