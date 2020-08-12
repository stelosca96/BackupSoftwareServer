//
// Created by stefano on 13/06/20.
//

#include <iostream>
#include "Jobs.h"

std::shared_ptr<Socket> Jobs::get() {
    //todo: fare la select
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(1000), [this](){return !queue.empty() || producer_ended;});
    if(queue.empty())
        return nullptr;
    std::shared_ptr<Socket> socket = queue.front();
//    while(!socket.select()) {
//        queue.pop();
//        queue.push(socket);
//        socket = queue.front();
//    }
    queue.pop();
    std::cout << "Queue size: " << queue.size() << std::endl;
    cv_put.notify_one();
    return socket;
}

void Jobs::put(std::shared_ptr<Socket> socket) {
    std::unique_lock<std::mutex> lock(mutex);
    std::cout << "Queue size: " << queue.size() << std::endl;
    cv_put.wait(lock, [this](){ return queue.size() < max_len;});
    queue.push(socket);
    std::cout << "Queue size: " << queue.size() << std::endl;
    cv.notify_one();
}

Jobs::Jobs(int maxLen) : max_len(maxLen) {}

void Jobs::producer_end() {
    std::lock_guard lock(mutex);
    producer_ended = true;
}

bool Jobs::producer_is_ended() {
    std::lock_guard lock(mutex);
    return producer_ended;
}


