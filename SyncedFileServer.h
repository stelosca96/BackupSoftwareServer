//
// Created by stefano on 06/08/20.
//

#ifndef FILEWATCHER_SYNCEDFILE_H
#define FILEWATCHER_SYNCEDFILE_H


#include <string>
#include <optional>
#include <boost/property_tree/ptree.hpp>

enum class FileStatus {created, modified, erased, not_valid};

class SyncedFileServer {
private:
    std::string path;
    std::string hash = "";
    unsigned long file_size = 0;
    bool is_file;

    FileStatus fileStatus = FileStatus::not_valid;

    // il file viene aggiunto alla coda di file da modificare,
    // fino a quando non si inizia il caricamento il file non viene riaggiunto
    // alla coda se sono effettuate ulteriori modifiche
    // la variabile is_syncing deve essere letta e modificata solo dalla classe
    // UploadJobs che la protegge con un lock nel caso ci sia un thread pool che effettua l'upload parallelo
    bool is_syncing = false;

public:
//    SyncedFileServer();

    explicit SyncedFileServer(const std::string& JSON);

//    SyncedFileServer(SyncedFileServer const &syncedFile);

    void update_file_data();
    static std::string CalcSha256(const std::string& filename);
    std::string to_string();

    //Deleted solo perchè non li usiamo e perchè evitiamo di usarli inconsciamente
    SyncedFileServer(SyncedFileServer const &syncedFile)=delete;
    SyncedFileServer(SyncedFileServer&& syncedFile)=delete;
    SyncedFileServer& operator=(const SyncedFileServer& syncedFile)=delete;
    SyncedFileServer&& operator=(SyncedFileServer&& syncedFile)=delete;

    bool operator==(const SyncedFileServer &rhs) const;
    bool operator!=(const SyncedFileServer &rhs) const;

    [[nodiscard]] const std::string &getPath() const;
    [[nodiscard]] const std::string &getHash() const;
    [[nodiscard]] FileStatus getFileStatus() const;
    [[nodiscard]] unsigned long getFileSize() const;
    [[nodiscard]] bool isSyncing() const;
    [[nodiscard]] bool isFile() const;

    void setSyncing(bool syncing);
    std::string getJSON();

    boost::property_tree::basic_ptree<std::string, std::string> getPtree();
};


#endif //FILEWATCHER_SYNCEDFILE_H
