
#ifndef DATABENTO_ORDERBOOK_ORDER_POOL_H
#define DATABENTO_ORDERBOOK_ORDER_POOL_H
#include "order.h"


class OrderPool {
private:
    std::vector<std::unique_ptr<Order>> pool;
    std::vector<Order*> available_orders;

public:
    explicit OrderPool(size_t);
    //~OrderPool();
    inline Order* get_order() {
        if (available_orders.empty()) {
            pool.push_back(std::make_unique<Order>());
            return pool.back().get();
        }
        Order* order = available_orders.back();
        available_orders.pop_back();
        return order;
    }

    inline void return_order(Order* order) {
        available_orders.push_back(order);

    }
};


#endif //DATABENTO_ORDERBOOK_ORDER_POOL_H
