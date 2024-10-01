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
#include "database.h"

template<bool Side>
struct BookSide {
};

template<>
struct BookSide<true> {
    using MapType = std::map<int32_t, Limit *, std::greater<>>;
    static constexpr bool is_bid = true;

};

template<>
struct BookSide<false> {
    using MapType = std::map<int32_t, Limit *, std::less<>>;
    static constexpr bool is_bid = false;
};


class Orderbook {
private:
    DatabaseManager &db_manager_;
    OrderPool order_pool_;
    std::unordered_map<std::pair<int32_t, bool>, Limit *, boost::hash<std::pair<int32_t, bool>>> limit_lookup_;
    uint64_t bid_count_;
    uint64_t ask_count_;

    template<bool Side>
    typename BookSide<Side>::MapType &get_book_side();

    template<bool Side>
    Limit *get_or_insert_limit(int32_t price);

    template<bool Side>
    void update_vol(int32_t price, int32_t size, bool is_add);

    template<bool Side>
    void update_modify_vol(int32_t og_price, int32_t new_prive, int32_t og_size, int32_t new_size);


public:
    bool update_possible = false;
    BookSide<true>::MapType bids_;
    BookSide<false>::MapType offers_;
    std::unordered_map<uint64_t, Order *> order_lookup_;
    std::chrono::system_clock::time_point current_message_time_;
    double vwap_, sum1_, sum2_;
    float skew_, bid_depth_, ask_depth_;
    int32_t bid_vol_, ask_vol_;
    std::string last_reset_time_;
    double imbalance_;

    explicit Orderbook(DatabaseManager &db_manager);

    ~Orderbook();

    void calculate_skew();

    void calculate_imbalance();

    uint64_t get_bid_depth() const;

    uint64_t get_ask_depth() const;

    template<bool Side>
    void add_limit_order(uint64_t id, int32_t price, uint32_t size, uint64_t unix_time);

    template<bool Side>
    void modify_order(uint64_t id, int32_t new_price, uint32_t new_size, uint64_t unix_time);

    template<bool Side>
    void trade_order(uint64_t id, int32_t price, uint32_t size);

    template<bool Side>
    void remove_order(uint64_t id, int32_t price, uint32_t size);

    template<bool Side>
    int32_t &get_volume();

    int32_t get_best_bid_price() const;

    int32_t get_best_ask_price() const;

    uint64_t get_count() const;


    inline void process_msg(const message &msg) {
        auto nanoseconds = std::chrono::nanoseconds(msg.time_);
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(nanoseconds);
        current_message_time_ = std::chrono::system_clock::time_point(microseconds);
        switch (msg.action_) {
            case 'A':
                msg.side_ ? add_limit_order<true>(msg.id_, msg.price_, msg.size_, msg.time_)
                          : add_limit_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                break;
            case 'C':
                msg.side_ ? remove_order<true>(msg.id_, msg.price_, msg.size_)
                          : remove_order<false>(msg.id_, msg.price_, msg.size_);
                break;
            case 'M':
                msg.side_ ? modify_order<true>(msg.id_, msg.price_, msg.size_, msg.time_)
                          : modify_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                break;
            case 'T':
                msg.side_ ? trade_order<true>(msg.id_, msg.price_, msg.size_)
                          : trade_order<false>(msg.id_, msg.price_, msg.size_);
                break;
        }

    }

    inline int32_t get_mid_price() {
        return (get_best_bid_price() + get_best_ask_price()) / 2;
    }

    inline void calculate_vols() {
        bid_vol_ = 0;
        ask_vol_ = 0;

        auto bid_it = bids_.begin();
        auto bid_end = bids_.end();
        for (int i = 0; i < 80 && bid_it != bid_end; i += 4) {
            if (bid_it != bid_end) {
                bid_vol_ += bid_it->second->volume_;
                ++bid_it;
            }
            if (bid_it != bid_end) {
                bid_vol_ += bid_it->second->volume_;
                ++bid_it;
            }
            if (bid_it != bid_end) {
                bid_vol_ += bid_it->second->volume_;
                ++bid_it;
            }
            if (bid_it != bid_end) {
                bid_vol_ += bid_it->second->volume_;
                ++bid_it;
            }
        }

        auto ask_it = offers_.begin();
        auto ask_end = offers_.end();
        for (int i = 0; i < 80 && ask_it != ask_end; i += 4) {
            if (ask_it != ask_end) {
                ask_vol_ += ask_it->second->volume_;
                ++ask_it;
            }
            if (ask_it != ask_end) {
                ask_vol_ += ask_it->second->volume_;
                ++ask_it;
            }
            if (ask_it != ask_end) {
                ask_vol_ += ask_it->second->volume_;
                ++ask_it;
            }
            if (ask_it != ask_end) {
                ask_vol_ += ask_it->second->volume_;
                ++ask_it;
            }
        }
        update_possible = true;
    }

    inline void calculate_vwap(int32_t price, int32_t size) {
        sum1_ += (double) (price * size);
        sum2_ += (double) size;
        vwap_ = sum1_ / sum2_;
    }

    std::string get_formatted_time_fast() const;

    std::string get_formatted_time() const;
};


#endif //DATABENTO_ORDERBOOK_ORDERBOOK_H
