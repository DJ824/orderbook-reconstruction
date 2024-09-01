//
// Created by Devang Jaiswal on 6/27/24.
//

#ifndef DATABENTO_ORDERBOOK_LIMIT_H
#define DATABENTO_ORDERBOOK_LIMIT_H

#include <cstdint>
#include "order.h"

class Limit {
public:
    explicit Limit(Order* new_order);
    Limit(float price);

    float price_;
    uint64_t volume_;
    uint32_t num_orders_;
    Order* head_;
    Order* tail_;
    uint64_t total_volume() { return volume_; }
    bool side_;

    void add_order(Order* new_order);
    void remove_order(Order* target);
    uint64_t get_price();
    uint64_t get_volume();
    uint32_t get_size();
    bool is_empty();
    void reset();
    void set(float price);
};


#endif //DATABENTO_ORDERBOOK_LIMIT_H
