//
// Created by Devang Jaiswal on 8/30/24.
//

#include "strategy.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>

class SkewStrategy : public Strategy {
private:
    int position_;
    int buy_qty_;
    int sell_qty_;
    float real_total_buy_px_;
    float real_total_sell_px_;
    float theo_total_buy_px_;
    float theo_total_sell_px_;
    float fees_;
    float pnl_;
    int max_pos_;
    const float POINT_VALUE_;
    const float FEES_PER_SIDE_;
    const float ALPHA_THRESHOLD_;
    std::vector<std::map<std::string, std::string>> results_;

public:
    SkewStrategy(float initial_capital = 1000000, float alpha_threshold = 1.7)
            : position_(0), buy_qty_(0), sell_qty_(0),
              real_total_buy_px_(0), real_total_sell_px_(0),
              theo_total_buy_px_(0), theo_total_sell_px_(0),
              fees_(0), pnl_(0), max_pos_(10), POINT_VALUE_(50.0f),
              FEES_PER_SIDE_(0.44f), ALPHA_THRESHOLD_(alpha_threshold) {}

    void on_book_update(const Orderbook& book) override {
        float bid_size = book.get_bid_depth();
        float ask_size = book.get_ask_depth();
        float bid_price = book.get_best_bid_price();
        float ask_price = book.get_best_ask_price();

        float skew = std::log10(bid_size) - std::log10(ask_size);

        if (skew > ALPHA_THRESHOLD_ && position_ < max_pos_) {
            execute_trade(true, ask_price, 1);
        } else if (skew < -ALPHA_THRESHOLD_ && position_ > -max_pos_) {
            execute_trade(false, bid_price, 1);
        }

        update_theo_values(book);
        calculate_pnl();
        store_results(book, skew);
        print_stats();
    }

    void execute_trade(bool is_buy, float price, int size) {
        if (is_buy) {
            position_ += size;
            buy_qty_ += size;
            real_total_buy_px_ += price * size;
        } else {
            position_ -= size;
            sell_qty_ += size;
            real_total_sell_px_ += price * size;
        }
        fees_ += FEES_PER_SIDE_ * size;
    }

    void update_theo_values(const Orderbook& book) {
        if (position_ == 0) {
            theo_total_buy_px_ = 0;
            theo_total_sell_px_ = 0;
        } else if (position_ > 0) {
            theo_total_sell_px_ = book.get_best_bid_price() * std::abs(position_);
            theo_total_buy_px_ = 0;
        } else if (position_ < 0) {
            theo_total_buy_px_ = book.get_best_ask_price() * std::abs(position_);
            theo_total_sell_px_ = 0;
        }
    }

    void calculate_pnl() {
        pnl_ = POINT_VALUE_ * (real_total_sell_px_ + theo_total_sell_px_ -
                              real_total_buy_px_ - theo_total_buy_px_) - fees_;
    }

    void store_results(const Orderbook& book, float skew) {
        std::map<std::string, std::string> result;
        result["ts_strategy"] = book.get_formatted_time();
        result["bid"] = std::to_string(book.get_best_bid_price());
        result["ask"] = std::to_string(book.get_best_ask_price());
        result["skew"] = std::to_string(skew);
        result["position"] = std::to_string(position_);
        result["trade_ct"] = std::to_string(buy_qty_ + sell_qty_);
        result["fees"] = std::to_string(fees_);
        result["pnl"] = std::to_string(pnl_);
        results_.push_back(result);
    }

    void print_stats() const {
        if (results_.empty()) return;

        const auto& result = results_.back();
        std::cout << "time: " << result.at("ts_strategy") << std::endl;
        std::cout << "bid: " << result.at("bid") << std::endl;
        std::cout << "ask: " << result.at("ask") << std::endl;
        std::cout << "skew: " << result.at("skew") << std::endl;
        std::cout << "position: " << result.at("position") << std::endl;
        std::cout << "trade count: " << result.at("trade_ct") << std::endl;
        std::cout << "fees: " << result.at("fees") << std::endl;
        std::cout << "pnL: " << result.at("pnl") << std::endl;
        std::cout << "-------------------------------------" << std::endl;
    }
};