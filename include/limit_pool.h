//
// Created by Devang Jaiswal on 7/4/24.
//


/*
#ifndef DATABENTO_ORDERBOOK_LIMIT_POOL_H
#define DATABENTO_ORDERBOOK_LIMIT_POOL_H

#include <unordered_map>
#include <list>
#include <memory>
#include "limit.h"


class limit_pool {
private:
    std::unordered_map<float,std::unique_ptr<limit>> limits_by_price_;

public:
    limit* acquire_limit(float price);
    void release_limit(limit* curr);
    //~limit_pool();
};


#endif //DATABENTO_ORDERBOOK_LIMIT_POOL_H
*/