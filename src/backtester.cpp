#include "backtester.h"
#include <iostream>
#include <cstring>

Backtester::Backtester(Orderbook &book, DatabaseManager &db_manager, QApplication &app,
                       const std::vector<message> &messages, QObject *parent)
        : QObject(parent), book_(book), db_manager_(db_manager), first_update_(false), current_message_index_(0),
          messages_(messages), running_(false) {
    std::cout << "Backtester constructed" << std::endl;
    chart_view_ = new BookGui(&window_);
    window_.setCentralWidget(chart_view_);
    window_.resize(800, 600);
    window_.show();

    QCoreApplication::processEvents();
}

Backtester::~Backtester() {
    strategies_.clear();
    std::cout << "Backtester destructed" << std::endl;
}

void Backtester::add_strategy(std::unique_ptr<Strategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

void Backtester::update_gui() {
    int32_t bid = book_.get_best_bid_price();
    int32_t ask = book_.get_best_ask_price();
    std::string time_string = book_.get_formatted_time_fast();

    QDateTime date_time = QDateTime::fromString(QString::fromStdString(time_string), "yyyy-MM-dd HH:mm:ss.zzz");
    qint64 timestamp = date_time.toMSecsSinceEpoch();

    chart_view_->add_data_point(timestamp, bid, ask);

    QCoreApplication::processEvents();
}

void Backtester::start_backtest() {
    running_ = true;
    QMetaObject::invokeMethod(this, "run_backtest", Qt::QueuedConnection);
}

void Backtester::stop_backtest() {
    running_ = false;
}

void Backtester::process_message(const message &msg) {
    book_.process_msg(msg);

    std::string curr_time = book_.get_formatted_time_fast();

    if (curr_time >= start_time_ && current_message_index_ % UPDATE_INTERVAL == 0) {
        update_gui();
    }

    if (current_message_index_ < 14000 && !first_update_) {
        book_.calculate_vols();
        first_update_ = true;
    }

    if (current_message_index_ % 1000 == 0) {
        book_.calculate_vols();
    }

    if (current_message_index_ % 100 == 0) {
        db_manager_.update_limit_orderbook(book_.bids_, book_.offers_);
    }

    if (curr_time >= start_time_) {
        book_.calculate_imbalance();
        for (auto &strategy: strategies_) {
            strategy->on_book_update(book_);
        }
    }

    int progress = static_cast<int>((static_cast<double>(current_message_index_) / messages_.size()) * 100);
    emit update_progress(progress);
}

void Backtester::run_backtest() {
    std::string prev_time;

    while (running_ && current_message_index_ < messages_.size()) {
        const auto &msg = messages_[current_message_index_];
        process_message(msg);

        std::string curr_time = book_.get_formatted_time_fast();
        if (curr_time >= end_time_) {
            break;
        }

        prev_time = curr_time;
        ++current_message_index_;
    }

    if (current_message_index_ >= messages_.size() || book_.get_formatted_time_fast() >= end_time_) {
        emit backtest_finished();
    }
}