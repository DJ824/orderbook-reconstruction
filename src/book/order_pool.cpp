
#include "order_pool.h"

OrderPool::OrderPool(size_t initial_size) {
    pool.reserve(initial_size);
    available_orders.reserve(initial_size);
    for (size_t i = 0; i < initial_size; ++i) {
        pool.push_back(std::make_unique<Order>());
        available_orders.push_back(pool.back().get());
    }
}


