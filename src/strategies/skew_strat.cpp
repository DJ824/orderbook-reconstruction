#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>
#include "strategy.h"
#include "../async_logger.cpp"
#include "../database.cpp"

class SkewStrategy : public Strategy {
private:
    int position_;
    int buy_qty_;
    int sell_qty_;
    int32_t real_total_buy_px_;
    int32_t real_total_sell_px_;
    int32_t theo_total_buy_px_;
    int32_t theo_total_sell_px_;
    float fees_;
    int32_t pnl_;
    int max_pos_;
    const int32_t POINT_VALUE_;
    const int32_t FEES_PER_SIDE_;
    int32_t prev_pnl_;
    const float ALPHA_THRESHOLD_;
    DatabaseManager& db_manager_;
    std::unique_ptr<AsyncLogger> logger_;

    void execute_trade(bool is_buy, int32_t price, int size) {
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
            theo_total_buy_px_ = theo_total_sell_px_ = 0;
        } else if (position_ > 0) {
            theo_total_sell_px_ = book.get_best_bid_price() * std::abs(position_);
            theo_total_buy_px_ = 0;
        } else {
            theo_total_buy_px_ = book.get_best_ask_price() * std::abs(position_);
            theo_total_sell_px_ = 0;
        }
    }

    void calculate_pnl() {
        pnl_ = POINT_VALUE_ * (real_total_sell_px_ + theo_total_sell_px_ -
                               real_total_buy_px_ - theo_total_buy_px_) - fees_;
    }

    void log_stats(const Orderbook& book) {
        std::string timestamp = book.get_formatted_time();
        auto bid = book.get_best_bid_price();
        auto ask = book.get_best_ask_price();
        int trade_count = buy_qty_ + sell_qty_;

        logger_->log(timestamp, bid, ask, position_, trade_count, pnl_);
        //db_manager_.log_trading_data(timestamp, bid, ask, position_, trade_count, pnl_, book.skew_);
    }

public:
    explicit SkewStrategy(DatabaseManager& db_manager)
            : position_(0), buy_qty_(0), sell_qty_(0),
              real_total_buy_px_(0), real_total_sell_px_(0),
              theo_total_buy_px_(0), theo_total_sell_px_(0),
              fees_(0), pnl_(0), prev_pnl_(0), max_pos_(50),
              POINT_VALUE_(5.0), FEES_PER_SIDE_(52),
              ALPHA_THRESHOLD_(1.7),
              db_manager_(db_manager) {
        logger_ = std::make_unique<AsyncLogger>("skew_strat_log.csv", db_manager, true);
    }

    void on_book_update(const Orderbook& book) override {
        try {
            float bid_size = book.get_bid_depth();
            float ask_size = book.get_ask_depth();
            float skew = std::log10(bid_size) - std::log10(ask_size);

            if (skew > ALPHA_THRESHOLD_ && position_ < max_pos_) {
                execute_trade(true, book.get_best_ask_price(), 1);
            } else if (skew < -ALPHA_THRESHOLD_ && position_ > -max_pos_) {
                execute_trade(false, book.get_best_bid_price(), 1);
            }

            update_theo_values(book);
            calculate_pnl();

            if (pnl_ != prev_pnl_) {
                log_stats(book);
                prev_pnl_ = pnl_;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};