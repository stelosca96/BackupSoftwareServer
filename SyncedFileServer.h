//
// Created by stefano on 06/08/20.
//

#ifndef FILEWATCHER_SYNCEDFILE_H
#define FILEWATCHER_SYNCEDFILE_H


#include <string>
#include <optional>

enum class FileStatus {created, modified, erased, not_valid};

class SyncedFileServer {
private:
    std::string path;
    std::string hash = "";
    unsigned long file_size = 0;
    bool is_file;

    // todo: il fileStatus serve veramente? magari distinguere tra modificato/eliminato/non_valido
    FileStatus fileStatus = FileStatus::not_valid;

    // il file viene aggiunto alla coda di file da modificare,
    // fino a quando non si inizia il caricamento il file non viene riaggiunto
    // alla coda se sono effettuate ulteriori modifiche
    // la variabile is_syncing deve essere letta e modificata solo dalla classe
    // UploadJobs che la protegge con un lock nel caso ci sia un thread pool che effettua l'upload parallelo
    bool is_syncing = false;

public:
    //todo: togliere costruttore vuoto
    SyncedFileServer();

    explicit SyncedFileServer(const std::string& JSON);

    SyncedFileServer(SyncedFileServer const &syncedFile);

    void update_file_data();
    static std::string CalcSha256(const std::string& filename);
    //todo: costruttore di copia e movimento
    std::string to_string();

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

};


#endif //FILEWATCHER_SYNCEDFILE_H
