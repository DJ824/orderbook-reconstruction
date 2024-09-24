#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include "strategy.h"
#include "../async_logger.cpp"

class ImbalanceStrat : public Strategy {
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
    DatabaseManager& db_manager_;
    std::unique_ptr<AsyncLogger> logger_;
    double imbalance_mean_ = 0.0;
    double imbalance_variance_ = 0.0;
    int update_count_ = 0;
    const int WARMUP_PERIOD = 1000;

    void update_imbalance_stats(double imabalance) {
        ++update_count_;
        if (update_count_ < WARMUP_PERIOD) {
            double delta = imabalance - imbalance_mean_;
            imbalance_mean_ += delta / update_count_;
            double delta2 = imabalance - imbalance_mean_;
            imbalance_variance_ += delta * delta2;
        } else {
            double delta = imabalance - imbalance_mean_;
            imbalance_mean_ += delta / WARMUP_PERIOD;
            double delta2 = imabalance - imbalance_mean_;
            imbalance_variance_ += (delta * delta2 - imbalance_variance_) / WARMUP_PERIOD;
        }
    }

    int calculate_trade_size(double imbalance) {
        if (update_count_ < WARMUP_PERIOD) return 1;

        double std_dev = std::sqrt(imbalance_variance_ / (WARMUP_PERIOD - 1));
        if (std_dev == 0) return 1;

        double normalized_imbalance = std::abs(imbalance - imbalance_mean_) / std_dev;
        double strength = std::min(normalized_imbalance, 1.0);

        int size = static_cast<int>(1 + 2 * strength);
        return std::min(size, 3);
    }

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
        std::string timestamp = book.get_formatted_time_fast();
        auto bid = book.get_best_bid_price();
        auto ask = book.get_best_ask_price();
        int trade_count = buy_qty_ + sell_qty_;

        logger_->log(timestamp, bid, ask, position_, trade_count, pnl_);
    }

public:
    explicit ImbalanceStrat(DatabaseManager& db_manager)
            : position_(0), buy_qty_(0), sell_qty_(0),
              real_total_buy_px_(0), real_total_sell_px_(0),
              theo_total_buy_px_(0), theo_total_sell_px_(0),
              fees_(0), pnl_(0), prev_pnl_(0), max_pos_(10),
              POINT_VALUE_(5.0), FEES_PER_SIDE_(100),
              db_manager_(db_manager) {
        logger_ = std::make_unique<AsyncLogger>("imbalance_strat_log.csv", db_manager, true);
    }

    void on_book_update(const Orderbook& book) override {
        try {
            auto imbalance = book.imbalance_;
            update_imbalance_stats(imbalance);

            auto mid_price = (double)(book.get_best_bid_price() + book.get_best_ask_price()) / 2.0;
            int trade_size = calculate_trade_size(imbalance);

            if (imbalance > 0 && mid_price < book.vwap_ && position_ + trade_size <= max_pos_) {
                execute_trade(true, book.get_best_ask_price(), trade_size);
            } else if (imbalance < 0 && mid_price > book.vwap_ && position_ - trade_size >= -max_pos_) {
                execute_trade(false, book.get_best_bid_price(), trade_size);
            }

            update_theo_values(book);
            calculate_pnl();

            if (pnl_ != prev_pnl_) {
                log_stats(book);
                prev_pnl_ = pnl_;
            }
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << std::endl;
        }
    }
};
