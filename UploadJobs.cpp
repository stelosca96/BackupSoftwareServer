//
// Created by stefano on 07/08/20.
//

#include <iostream>
#include "UploadJobs.h"


std::shared_ptr<SyncedFileServer> UploadJobs::get() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(3000), [this](){return !queue.empty() || producer_ended;});
    if(queue.empty())
        return nullptr;
    std::shared_ptr<SyncedFileServer> sfp = queue.front();
    queue.pop();
    sfp->setSyncing(false);
    std::cout << "Queue size: " << queue.size() << std::endl;
    cv_put.notify_one();
    return sfp;
}

void UploadJobs::put(const std::shared_ptr<SyncedFileServer>& sfp) {
    std::unique_lock<std::mutex> lock(mutex);
    if(!sfp->isSyncing()){
        std::cout << "Queue size: " << queue.size() << std::endl;
        queue.push(sfp);
        std::cout << "Queue size: " << queue.size() << std::endl;
        sfp->setSyncing(true);
    }
    cv.notify_one();
}

void UploadJobs::producer_end() {
    std::lock_guard lock(mutex);
    producer_ended = true;
}

bool UploadJobs::producer_is_ended() {
    std::lock_guard lock(mutex);
    return producer_ended;
}

int UploadJobs::queue_size() {
    std::lock_guard lock(mutex);
    return queue.size();
}
