//
// Created by Devang Jaiswal on 6/27/24.
//
#include <iostream>
#include "orderbook.h"


orderbook::orderbook() : bids_(), offers_(), order_lookup_(), limit_pool_() {
    best_bid_it_ = bids_.begin();
    best_offer_it_ = offers_.begin();
    bid_count_ = 0;
    ask_count_ = 0;
};

void orderbook::add_limit_order(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time) {
    order *new_order = new order(id, price, size, side, unix_time);
    limit* curr_limit = nullptr;
    if (side) {
        if (price == best_bid_it_->first) {
            curr_limit = best_bid_it_->second;
        } else {
            curr_limit = get_or_insert_limit(side, price);
        }
    } else {
        if (price == best_offer_it_->first) {
            curr_limit = best_offer_it_->second;
        } else {
            curr_limit = get_or_insert_limit(side, price);
        }
    }
    order_lookup_[id] = new_order;
    curr_limit->add_order(new_order);
    curr_limit->price_ = price;
    if (side) {
        ++bid_count_;
    } else {
        ++ask_count_;
    }
    std::cout << "added limit order with id: " << id << " size: " << size << " price: " << price << std::endl;

    update_best_bid();
    update_best_offer();
}

void orderbook::remove_order(uint64_t id, float price, uint32_t size, bool side) {
    order *target = order_lookup_[id];
    auto curr_limit = target->parent_;
    order_lookup_.erase(id);
    curr_limit->remove_order(target);
    if (curr_limit->is_empty()) {
        if (target->side_) {
            bids_.erase(price);
        } else {
            offers_.erase(price);
        }
        limit_pool_.release_limit(curr_limit);
        target->parent_ = nullptr;
    }

    if (side) {
        --bid_count_;
    } else {
        --ask_count_;
    }
    delete target;

    update_best_bid();
    update_best_offer();


    std::cout << "cancelled limit order with id: " << id << " size: " << size << " price: " << price << std::endl;
}


void orderbook::modify_order(uint64_t id, float new_price, uint32_t new_size, bool side, uint64_t unix_time) {
    if (order_lookup_.find(id) == order_lookup_.end()) {
        add_limit_order(id, new_price, new_size, side, unix_time);
        return;
    }

    order *target = order_lookup_[id];
    if (target->side_ != side) {
        throw std::logic_error("cannot switch sides");
    }

    auto prev_price = target->price_;
    auto prev_limit = target->parent_;
    auto prev_size = target->size;
    if (prev_price != new_price) {
        prev_limit->remove_order(target);
        if (prev_limit->is_empty()) {
            if (side) {
                bids_.erase(prev_price);
            } else {
                offers_.erase(prev_price);
            }
            limit_pool_.release_limit(prev_limit);
        }
        limit *new_limit = get_or_insert_limit(side, new_price);
        target->size = new_size;
        target->price_ = new_price;
        target->unix_time_ = unix_time;
        new_limit->add_order(target);
    } else if (prev_size < new_size) {
        prev_limit->remove_order(target);
        target->size = new_size;
        target->unix_time_ = unix_time;
        prev_limit->add_order(target);
    } else {
        target->size = new_size;
        target->unix_time_ = unix_time;
    }
    std::cout << "modified order with id: " << id << std::endl;
    std::cout << "old price: " << prev_price << " new price: " << target->price_ << std::endl;
    std::cout << "old size: " << prev_size << " new size: " << target->size << std::endl;
    update_best_bid();
    update_best_offer();
}


void orderbook::trade_order(uint64_t id, float price, uint32_t size, bool side) {
    if (side) {
        if (offers_.empty()) {
            return;
        }

        auto trade_limit = get_or_insert_limit(false, price);

        auto offer_it = trade_limit->head_;
        while (offer_it != nullptr) {
            if (size == offer_it->size) {
                offer_it->filled_ = true;
                std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                          << offer_it->price_ << std::endl;
                size = 0;
                break;
            } else if (size < offer_it->size) {
                offer_it->size -= size;
                std::cout << "market order filled" << std::endl;
                std::cout << "size reduced for order id: " << offer_it->id_ << " new size: " << offer_it->size
                          << std::endl;
                size = 0;
                break;
            } else {
                size -= offer_it->size;
                offer_it->filled_ = true;
                if (size == 0) {
                    std::cout << "market order fully filled" << std::endl;
                    std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                              << offer_it->price_ << std::endl;
                    break;
                }
                std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                          << offer_it->price_ << std::endl;

                offer_it = offer_it->next_;
            }
        }

    } else {
        if (bids_.empty()) {
            return;
        }

        auto trade_limit = get_or_insert_limit(true, price);

        auto bid_it = trade_limit->head_;
        while (bid_it != nullptr) {
            if (size == bid_it->size) {
                bid_it->filled_ = true;
                std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                          << bid_it->price_ << std::endl;
                size = 0;
                break;
            } else if (size < bid_it->size) {
                bid_it->size -= size;
                std::cout << "market order filled" << std::endl;
                std::cout << "size reduced for order id: " << bid_it->id_ << " new size: " << bid_it->size << std::endl;
                size = 0;
                break;
            } else {
                size -= bid_it->size;
                bid_it->filled_ = true;
                if (size == 0) {
                    std::cout << "market order filled" << std::endl;
                    std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                              << bid_it->price_ << std::endl;
                    break;
                }
                std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                          << bid_it->price_ << std::endl;
                bid_it = bid_it->next_;
            }

        }
    }
    update_best_bid();
    update_best_offer();
}


limit *orderbook::get_or_insert_limit(bool side, float price) {
    if (side) {
        limit* curr = bids_[price];
        if (curr == nullptr) {
           limit* new_limit = limit_pool_.acquire_limit(price);
           bids_[price] = new_limit;
           return new_limit;
        } else return curr;
    } else {
        limit* curr = offers_[price];
        if (curr == nullptr) {
            limit* new_limit = limit_pool_.acquire_limit(price);
            offers_[price] = new_limit;
            return new_limit;
        } else return curr;
    }

    update_best_bid();
    update_best_offer();
}


void orderbook::update_best_bid() {
    if (!bids_.empty()) {
        best_bid_it_ = bids_.begin();
    }
}

void orderbook::update_best_offer()  {
    if (!offers_.empty()) {
        best_offer_it_ = offers_.begin();
    }
}

float orderbook::get_best_bid_price() { return bids_[0]->price_; }

float orderbook::get_best_ask_price() { return offers_[0]->price_; }

limit* orderbook::get_best_bid() { return bids_[0]; }

limit* orderbook::get_best_ask() { return offers_[0]; }

uint64_t orderbook::get_count() { return bid_count_ + ask_count_; }

