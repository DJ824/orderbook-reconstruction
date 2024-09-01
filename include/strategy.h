//
// Created by Devang Jaiswal on 8/29/24.
//
#include "orderbook.h"
#pragma once
class Strategy {
public:
    virtual void on_book_update(const Orderbook& book) = 0;
    virtual ~Strategy() = default;
};