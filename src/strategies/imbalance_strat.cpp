#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include "strategy.h"
#include "../async_logger.cpp"
#include "../database.cpp"

class ImbalanceStrat : public Strategy {
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
    std::vector<std::map<std::string, std::string>> results_;
    int count_ = 0;
    float prev_pnl = 0;
    DatabaseManager& db_manager_;
    std::unique_ptr<AsyncLogger> logger_;




public:
    ImbalanceStrat(DatabaseManager& db_manager)
            : position_(0), buy_qty_(0), sell_qty_(0),
              real_total_buy_px_(0), real_total_sell_px_(0),
              theo_total_buy_px_(0), theo_total_sell_px_(0),
              fees_(0), pnl_(0), max_pos_(50), POINT_VALUE_(5.0f),
              FEES_PER_SIDE_(0.44f), db_manager_(db_manager) {
        logger_ = std::make_unique<AsyncLogger>("imbalance_strat_log.csv", true);  // true enables console output
    }

    void on_book_update(const Orderbook& book) override {
        float imbalance = book.imbalance_;
        float mid_price = (book.get_best_bid_price() + book.get_best_ask_price()) / 2.0f;

        if (imbalance > 0 && mid_price < book.vwap_ && position_ < max_pos_) {
            execute_trade(true, book.get_best_ask_price(), 1);
        } else if (imbalance < 0 && mid_price > book.vwap_ && position_ > -max_pos_) {
            execute_trade(false, book.get_best_bid_price(), 1);
        }

        update_theo_values(book);
        calculate_pnl();

        if (prev_pnl != pnl_) {
            log_stats(book);
            prev_pnl = pnl_;
        }

        //store_results(book);
        //print_results();
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
/*
    void store_results(const Orderbook& book) {
        std::map<std::string, std::string> result;
        result["ts_strategy"] = book.get_formatted_time();
        result["bid"] = std::to_string(book.get_best_bid_price());
        result["ask"] = std::to_string(book.get_best_ask_price());
        result["imbalance"] = std::to_string(book.imbalance_);
        result["position"] = std::to_string(position_);
        result["trade_ct"] = std::to_string(buy_qty_ + sell_qty_);
        result["fees"] = std::to_string(fees_);
        result["pnl"] = std::to_string(pnl_);
        results_.push_back(result);
    }

    void print_results() const {
        for (const auto& result : results_) {
            std::cout << "timestamp: " << result.at("ts_strategy") << "\n";
            std::cout << "bid: " << result.at("bid") << ", ask: " << result.at("ask") << "\n";
            std::cout << "imbalance: " << result.at("imbalance") << "\n";
            std::cout << "position: " << result.at("position") << "\n";
            std::cout << "trade Count: " << result.at("trade_ct") << "\n";
            std::cout << "fees: " << result.at("fees") << "\n";
            std::cout << "pnl: " << result.at("pnl") << "\n";
            std::cout << "------------------------------\n";
        }
    }
*/

    void log_stats(const Orderbook& book) {
        logger_->log(
                book.get_formatted_time(),
                book.get_best_bid_price(),
                book.get_best_ask_price(),
                position_,
                buy_qty_ + sell_qty_,
                pnl_
        );
    }

};