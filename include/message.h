//
// Created by Devang Jaiswal on 7/29/24.
//

#ifndef DATABENTO_ORDERBOOK_MESSAGE_H
#define DATABENTO_ORDERBOOK_MESSAGE_H
#include <cstdint>
struct message {
    uint64_t id_;
    uint64_t time_;
    uint32_t size_;
    float price_;
    char action_;
    bool side_;

    message(uint64_t id, uint64_t time, uint32_t size, float price, char action, bool side)
            : id_(id), time_(time), size_(size), price_(price), action_(action), side_(side) {}
};

#endif //DATABENTO_ORDERBOOK_MESSAGE_H
