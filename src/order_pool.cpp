//
// Created by Devang Jaiswal on 7/23/24.
//

#include "../include/order_pool.h"

order_pool::order_pool(size_t initial_size) : next_available(0) {
    pool.reserve(initial_size);
    for (size_t i = 0; i < initial_size; ++i) {
        pool.push_back(new order());
    }
}

order_pool::~order_pool() {
    for (auto* order : pool) {
        delete order;
    }
}

order *order_pool::get_order() {
    if (next_available >= pool.size()) {
        size_t current_size = pool.size();
        pool.reserve(current_size * 2);
        for (size_t i = 0; i < current_size; ++i) {
            pool.push_back(new order());
        }
    }
    return pool[next_available++];
}

void order_pool::return_order(order *order) {
    if (next_available > 0) {
        --next_available;
        pool[next_available] = order;
    }
}
