//
// Created by Devang Jaiswal on 6/27/24.
//

#ifndef DATABENTO_ORDERBOOK_ORDER_H
#define DATABENTO_ORDERBOOK_ORDER_H

#include <cstdint>
#include <string>
#include <ctime>

using std::string;

class Limit;

class Order {
public:
    Order(uint64_t order_id, float price, uint32_t order_size, bool order_side, uint64_t order_unix_time);
    Order();
    uint64_t id_;
    float price_;
    uint32_t size;
    bool side_;
    uint64_t unix_time_;
    Order* next_;
    Order* prev_;
    Limit* parent_;
    bool filled_;
};

#endif // DATABENTO_ORDERBOOK_ORDER_H
