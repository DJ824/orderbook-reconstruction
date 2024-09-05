//
// Created by Devang Jaiswal on 7/23/24.
//

#include "order_pool.h"

OrderPool::OrderPool(size_t initial_size) {
    pool.reserve(initial_size);
    available_orders.reserve(initial_size);
    for (size_t i = 0; i < initial_size; ++i) {
        pool.push_back(std::make_unique<Order>());
        available_orders.push_back(pool.back().get());
    }
}

Order *OrderPool::get_order() {
    if (available_orders.empty()) {
        pool.push_back(std::make_unique<Order>());
        return pool.back().get();
    }
    Order* order = available_orders.back();
    available_orders.pop_back();
    return order;
}

void OrderPool::return_order(Order *order) {
    available_orders.push_back(order);
}
