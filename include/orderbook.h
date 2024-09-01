//
// Created by Devang Jaiswal on 6/27/24.
//

#ifndef DATABENTO_ORDERBOOK_ORDERBOOK_H
#define DATABENTO_ORDERBOOK_ORDERBOOK_H

#include <cstdint>
#include <map>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <utility>
#include <iterator>
#include "order.h"
#include "limit.h"
#include "limit_pool.h"
#include "order_pool.h"
#include "message.h"
#include "../src/database.cpp"


class Orderbook {
private:
    std::unordered_map<std::pair<float, bool>, Limit*, boost::hash<std::pair<float, bool>>> limit_lookup_;
    OrderPool order_pool_;
    uint64_t bid_count_;
    uint64_t ask_count_;

    //limit_pool limit_pool_;
    DatabaseManager &db_manager_;

public:
    explicit Orderbook(DatabaseManager& db_manager);
    ~Orderbook();
    std::unordered_map<uint64_t , Order* > order_lookup_;
    std::chrono::system_clock::time_point current_message_time_;
    // vwap calculations
    float vwap_;
    float sum1_;
    float sum2_;

    // skew calculations
    float skew_;
    float bid_depth_;
    float ask_depth_;
    void calculate_skew();

    // imbalance calculations
    uint64_t bid_vol_;
    uint64_t ask_vol_;
    float imbalance_;
    void calculate_imbalance();
    void calculate_vols();


    uint64_t get_bid_depth() const;
    uint64_t get_ask_depth() const;


    void add_limit_order(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time);
    void modify_order(uint64_t id, float new_price, uint32_t new_size, bool side, uint64_t unix_time);
    void trade_order(uint64_t id, float price, uint32_t size, bool side);
    void remove_order(uint64_t id, float price, uint32_t size, bool side);
    Limit* get_best_bid();
    Limit* get_best_ask();
    float get_best_bid_price() const;
    float get_best_ask_price() const;
    uint64_t get_count() const;
    bool contains_order(uint64_t id);
    Order* get_order(uint64_t id);
    Limit* get_or_insert_limit(bool side, float price);

    inline void process_msg(const message &msg) {
        auto nanoseconds = std::chrono::nanoseconds(msg.time_);
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(nanoseconds);
        current_message_time_ = std::chrono::system_clock::time_point(microseconds);
        switch (msg.action_) {
            case 'A':
                add_limit_order(msg.id_, msg.price_, msg.size_, msg.side_, msg.time_);
                break;
            case 'C':
                remove_order(msg.id_, msg.price_, msg.size_, msg.side_);
                break;
            case 'M':
                modify_order(msg.id_, msg.price_, msg.size_, msg.side_, msg.time_);
                break;
            case 'T':
                trade_order(msg.id_, msg.price_, msg.size_, msg.side_);
                break;
        }
    }

    std::atomic<uint64_t> message_count_{0};
    std::map<float, Limit *, std::less<>> offers_;
    std::map<float, Limit *, std::greater<>> bids_;
    std::string get_formatted_time() const;

};


#endif //DATABENTO_ORDERBOOK_ORDERBOOK_H
