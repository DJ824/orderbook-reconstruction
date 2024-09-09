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
    std::unordered_map<std::pair<int32_t , bool>, Limit*, boost::hash<std::pair<int32_t, bool>>> limit_lookup_;
    OrderPool order_pool_;
    uint64_t bid_count_;
    uint64_t ask_count_;

    //limit_pool limit_pool_;
    DatabaseManager &db_manager_;
    bool update_possible = false;


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
    int32_t bid_vol_;
    int32_t ask_vol_;

    std::string last_reset_time_;

    double imbalance_;
    void calculate_imbalance();

    uint64_t get_bid_depth() const;
    uint64_t get_ask_depth() const;

    void add_limit_order(uint64_t id, int32_t price, uint32_t size, bool side, uint64_t unix_time);
    void modify_order(uint64_t id, int32_t new_price, uint32_t new_size, bool side, uint64_t unix_time);
    void trade_order(uint64_t id, int32_t price, uint32_t size, bool side);
    void remove_order(uint64_t id, int32_t price, uint32_t size, bool side);
    Limit* get_best_bid();
    Limit* get_best_ask();
    int32_t get_best_bid_price() const;
    int32_t get_best_ask_price() const;
    uint64_t get_count() const;
    bool contains_order(uint64_t id);
    Order* get_order(uint64_t id);
    Limit* get_or_insert_limit(bool side, int32_t price);


    inline void process_msg(const message &msg) {
        auto nanoseconds = std::chrono::nanoseconds(msg.time_);
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(nanoseconds);
        current_message_time_ = std::chrono::system_clock::time_point(microseconds);
        switch (msg.action_) {
            case 'A':
                //std::cout << msg.price_ << std::endl;
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



    inline void calculate_vols() {
        bid_vol_ = 0;
        ask_vol_ = 0;

        auto bid_it = bids_.begin();
        auto bid_end = bids_.end();
        for (int i = 0; i < 80 && bid_it != bid_end; ++i, ++bid_it) {
            bid_vol_ += bid_it->second->volume_;
        }

        auto ask_it = offers_.begin();
        auto ask_end = offers_.end();
        for (int i = 0; i < 80 && ask_it != ask_end; ++i, ++ask_it) {
            ask_vol_ += ask_it->second->volume_;
        }
        update_possible = true;

    }

    inline void update_vol(int32_t price, int32_t size, bool side, bool is_add, bool update) {
        if (!update) {
            return;
        }

        int32_t& vol = side ? bid_vol_ : ask_vol_;
        int32_t best_price = side ? get_best_bid_price() : get_best_ask_price();

        if (std::abs(price - best_price) <= 2000) {
            vol += is_add ? size : -size;
        }
    }


    inline void update_modify_vol(int32_t og_price, int32_t new_price, int32_t og_size, int32_t new_size, bool side, bool update) {
        if (!update) {
            return;
        }

        int32_t best_price = side ? get_best_bid_price() : get_best_ask_price();
        int32_t& vol = side ? bid_vol_ : ask_vol_;

        int32_t og_in_range = (std::abs(og_price - best_price) <= 2000);
        int32_t new_in_range = (std::abs(new_price - best_price) <= 2000);

        vol -= og_size * og_in_range;
        vol += new_size * new_in_range;
    }

    std::map<int32_t, Limit *, std::less<>> offers_;
    std::map<int32_t, Limit *, std::greater<>> bids_;
    std::string get_formatted_time_fast() const;
    std::string get_formatted_time() const;

};


#endif //DATABENTO_ORDERBOOK_ORDERBOOK_H
