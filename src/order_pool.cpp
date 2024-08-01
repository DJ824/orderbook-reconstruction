//
// Created by Devang Jaiswal on 7/23/24.
//

#include "../include/order_pool.h"

order_pool::order_pool(size_t initial_size) {
    pool.reserve(initial_size);
    available_orders.reserve(initial_size);
    for (size_t i = 0; i < initial_size; ++i) {
        pool.push_back(std::make_unique<order>());
        available_orders.push_back(pool.back().get());
    }

}

order *order_pool::get_order() {
    if (available_orders.empty()) {
        pool.push_back(std::make_unique<order>());
        return pool.back().get();
    }
    order* ord = available_orders.back();
    available_orders.pop_back();
    return ord;
}

void order_pool::return_order(order *order) {
    available_orders.push_back(order);
}