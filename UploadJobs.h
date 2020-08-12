//
// Created by stefano on 07/08/20.
//

#ifndef FILEWATCHER_UPLOADJOBS_H
#define FILEWATCHER_UPLOADJOBS_H


#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "SyncedFileServer.h"

//todo: forse dovrei lavorare con dei weak ptr, devo considerare il caso in cui il file sia eliminato prima di caricarlo sul server
class UploadJobs {
private:
    std::queue<std::shared_ptr<SyncedFileServer>> queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable cv_put;
    bool producer_ended = false;
public:
    std::shared_ptr<SyncedFileServer> get();
    void put(const std::shared_ptr<SyncedFileServer>& sfp);
    void producer_end();
    bool producer_is_ended();
    int queue_size();
};

#endif //FILEWATCHER_UPLOADJOBS_H
