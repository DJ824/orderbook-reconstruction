//
// Created by Devang Jaiswal on 6/27/24.
//
#include <iostream>
#include <iomanip>
#include <numeric>


#include "orderbook.h"

Orderbook::Orderbook(DatabaseManager& db_manager) : bids_(), offers_(), order_lookup_(), limit_lookup_(), order_pool_(1000000), db_manager_(db_manager)  {
    bid_count_ = 0;
    ask_count_ = 0;

    bids_.get_allocator().allocate(200);
    offers_.get_allocator().allocate(200);
    order_lookup_.reserve(1000000);
};

Orderbook::~Orderbook() {
    for (auto& pair : bids_) {
        delete pair.second;
    }
    for (auto& pair : offers_) {
        delete pair.second;
    }
}

void Orderbook::add_limit_order(uint64_t id, int32_t price, uint32_t size, bool side, uint64_t unix_time) {
    //order *new_order = new order(id, price, size, side, unix_time);

    Order* new_order = order_pool_.get_order();
    new_order->id_ = id;
    new_order->price_ = price;
    new_order->size = size;
    new_order->side_ = side;
    new_order->unix_time_ = unix_time;

    Limit* curr_limit = get_or_insert_limit(side, price);
    order_lookup_[id] = new_order;
    curr_limit->add_order(new_order);
    //curr_limit->price_ = price;
    if (side) {
        ++bid_count_;
    } else {
        ++ask_count_;
    }

    update_vol(price, size, side, true, update_possible);


    //db_manager_.add_order(id, price, size, side, unix_time);

    // std::cout << "added Limit order with id: " << id << " size: " << size << " price: " << price << std::endl;
}

void Orderbook::remove_order(uint64_t id, int32_t price, uint32_t size, bool side) {
    Order *target = order_lookup_[id];
    auto curr_limit = target->parent_;
    order_lookup_.erase(id);
    curr_limit->remove_order(target);
    if (curr_limit->is_empty()) {
        if (target->side_) {
            bids_.erase(price);
        } else {
            offers_.erase(price);
        }
        std::pair<int32_t , bool> key = std::make_pair(price, side);
        limit_lookup_.erase(key);
        target->parent_ = nullptr;
    }

    if (side) {
        --bid_count_;
    } else {
        --ask_count_;
    }
    //delete target;

    update_vol(price, size, side, false, update_possible);

    order_pool_.return_order(target);

    // db_manager_.remove_order(id);

    //std::cout << "cancelled Limit order with id: " << id << " size: " << size << " price: " << price << std::endl;
}


void Orderbook::modify_order(uint64_t id, int32_t new_price, uint32_t new_size, bool side, uint64_t unix_time) {
    if (order_lookup_.find(id) == order_lookup_.end()) {
        add_limit_order(id, new_price, new_size, side, unix_time);
        return;
    }

    Order *target = order_lookup_[id];
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
            std::pair<int32_t , bool> key = std::make_pair(prev_price, side);
            limit_lookup_.erase(key);
        }
        Limit *new_limit = get_or_insert_limit(side, new_price);
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

    update_modify_vol(prev_price, new_price, prev_size, new_size, side, update_possible);

    /*
    std::cout << "modified order with id: " << id << std::endl;
    std::cout << "old price: " << prev_price << " new price: " << target->price_ << std::endl;
    std::cout << "old size: " << prev_size << " new size: " << target->size << std::endl;
     */
    //db_manager_.modify_order(id, new_price, new_size, unix_time);
}


void Orderbook::trade_order(uint64_t id, int32_t price, uint32_t size, bool side) {
    auto og_size = size;
    if (side) {
        if (offers_.empty()) {
            return;
        }
        auto trade_limit = get_or_insert_limit(false, price);
        auto offer_it = trade_limit->head_;
        while (offer_it != nullptr) {
            if (size == offer_it->size) {
                offer_it->filled_ = true;
                /*
                std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                          << offer_it->price_ << std::endl;
                          */
                size = 0;
                break;
            } else if (size < offer_it->size) {
                offer_it->size -= size;
                ask_vol_ -= size;
                /*
                std::cout << "market order filled" << std::endl;
                std::cout << "size reduced for order id: " << offer_it->id_ << " new size: " << offer_it->size
                          << std::endl;
                          */
                size = 0;
                break;
            } else {
                size -= offer_it->size;
                offer_it->filled_ = true;
                if (size == 0) {
                    /*
                    std::cout << "market order fully filled" << std::endl;
                    std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                              << offer_it->price_ << std::endl;
                              */
                    break;
                }
                /*
                std::cout << "filled order with id: " << offer_it->id_ << " size: " << offer_it->size << " price: "
                          << offer_it->price_ << std::endl;
                */
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
                /*
                std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                          << bid_it->price_ << std::endl;
                          */
                size = 0;
                break;
            } else if (size < bid_it->size) {
                bid_it->size -= size;
                bid_vol_ -= size;
                /*
                std::cout << "market order filled" << std::endl;
                std::cout << "size reduced for order id: " << bid_it->id_ << " new size: " << bid_it->size << std::endl;
                 */
                size = 0;
                break;
            } else {
                size -= bid_it->size;
                bid_it->filled_ = true;
                if (size == 0) {
                    /*
                    std::cout << "market order filled" << std::endl;
                    std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                              << bid_it->price_ << std::endl;
                              */
                    break;
                }
                /*
                std::cout << "filled order with id: " << bid_it->id_ << " size: " << bid_it->size << " price: "
                          << bid_it->price_ << std::endl;
                */
                 bid_it = bid_it->next_;
            }
        }
    }


    //db_manager_.send_market_order_to_gui(price, og_size, side, unix_time);
    sum1_ += ((float) og_size * price);
    sum2_ += ((float) og_size);
    vwap_ = sum1_ / sum2_;
}

Limit *Orderbook::get_or_insert_limit(bool side, int32_t price) {
    std::pair<int32_t, bool> key = std::make_pair(price, side);
    auto it = limit_lookup_.find(key);
    if (it == limit_lookup_.end()) {
        auto* new_limit = new Limit(price);
        if (side) {
            bids_[price] = new_limit;
            new_limit->side_ = true;
            //update_best_bid();
        } else {
            offers_[price] = new_limit;
            new_limit->side_ = false;
            //update_best_offer();
        }
        limit_lookup_[key] = new_limit;
        return new_limit;
    } else {
        return it->second;
    }
}

std::string Orderbook::get_formatted_time_fast() const {
    static thread_local char buffer[32];
    static thread_local time_t last_second = 0;
    static thread_local char last_second_str[20];

    auto now = std::chrono::system_clock::to_time_t(current_message_time_);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_message_time_.time_since_epoch()) % 1000;

    if (now != last_second) {
        last_second = now;
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);  // Use localtime_r instead of gmtime_r
        strftime(last_second_str, sizeof(last_second_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
    }

    snprintf(buffer, sizeof(buffer), "%s.%03d", last_second_str, static_cast<int>(ms.count()));
    return std::string(buffer);
}


std::string Orderbook::get_formatted_time() const {
    auto time_t = std::chrono::system_clock::to_time_t(current_message_time_);
    std::tm* tm = std::localtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_message_time_.time_since_epoch()) % 1000;
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}


void Orderbook::calculate_skew() {
    skew_ = log10(get_bid_depth()) - log10(get_ask_depth());

}

void Orderbook::calculate_imbalance() {
    uint64_t total_vol = bid_vol_ + ask_vol_;
    if (total_vol == 0) {
        imbalance_ = 0.0;
        return;
    }
    imbalance_ = static_cast<double>(static_cast<int64_t>(bid_vol_) - static_cast<int64_t>(ask_vol_)) / static_cast<double>(total_vol);
}



int32_t Orderbook::get_best_bid_price() const { return bids_.begin()->first; }

int32_t Orderbook::get_best_ask_price() const { return offers_.begin()->first; }

Limit* Orderbook::get_best_bid() { return bids_[0]; }

Limit* Orderbook::get_best_ask() { return offers_[0]; }

uint64_t Orderbook::get_count() const { return bid_count_ + ask_count_; }

uint64_t Orderbook::get_bid_depth() const { return bids_.begin()->second->volume_; }

uint64_t Orderbook::get_ask_depth() const { return offers_.begin()->second->volume_; }

