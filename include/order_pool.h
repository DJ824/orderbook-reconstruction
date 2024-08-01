//
// Created by Devang Jaiswal on 7/23/24.
//


#ifndef DATABENTO_ORDERBOOK_ORDER_POOL_H
#define DATABENTO_ORDERBOOK_ORDER_POOL_H
#include "order.h"


class order_pool {
private:
    std::vector<std::unique_ptr<order>> pool;
    std::vector<order*> available_orders;

public:
    order_pool(size_t);
    //~order_pool();
    order* get_order();
    void return_order(order* order);
};


#endif //DATABENTO_ORDERBOOK_ORDER_POOL_H
