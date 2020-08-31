////
//// Created by stefano on 13/06/20.
////
//
//#ifndef PDS_LAB5_JOBS_H
//#define PDS_LAB5_JOBS_H
//
//#include <queue>
//#include <mutex>
//#include <condition_variable> // std::condition_variable
//#include "Socket.h"
//
//class Jobs {
//
//private:
//    std::queue<std::shared_ptr<Socket>> queue;
//    std::mutex mutex;
//    std::condition_variable cv;
//    std::condition_variable cv_put;
//    bool producer_ended = false;
//    unsigned int max_len = 10;
//
//public:
//    Jobs(unsigned int maxLen);
//    std::shared_ptr<Socket> get();
//    void put(std::shared_ptr<Socket> socket);
//    void producer_end();
//    bool producer_is_ended();
//};
//
//#endif //PDS_LAB5_JOBS_H
