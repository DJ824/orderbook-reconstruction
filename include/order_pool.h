
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
    Order* get_order();
    void return_order(Order* order);
};


#endif //DATABENTO_ORDERBOOK_ORDER_POOL_H
