#include "backtester.h"
#include <QDateTime>
#include <QCoreApplication>

Backtester::Backtester(Orderbook &book, DatabaseManager &db_manager,
                       const std::vector<message> &messages, QObject *parent)
        : QObject(nullptr), book_(book), db_manager_(db_manager), messages_(messages),
          first_update_(false), current_message_index_(0), running_(false) {

    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
             << "[Backtester] Backtester constructed on thread:" << QThread::currentThreadId();

    backtest_timer_ = new QTimer(this);
    connect(backtest_timer_, &QTimer::timeout, this, &Backtester::run_backtest);
}


Backtester::~Backtester() {
    if (worker_thread_.isRunning()) {
        worker_thread_.quit();
        worker_thread_.wait();
    }
}

void Backtester::add_strategy(std::unique_ptr<Strategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

void Backtester::handleStartSignal() {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
             << "[Backtester] handleStartSignal called on thread:" << QThread::currentThreadId();
    start_backtest();
}

void Backtester::start_backtest() {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
             << "[Backtester] start_backtest called on thread:" << QThread::currentThreadId();

    if (!running_) {
        running_ = true;
        if (current_message_index_ == 0) {
            qDebug() << "[Backtester] Starting from the beginning...";
        } else if (current_message_index_ >= messages_.size()) {
            qDebug() << "[Backtester] Already completed. Not restarting.";
            return;
        } else {
            qDebug() << "[Backtester] Resuming from message index:" << current_message_index_;
        }

        backtest_timer_->start(0);
        emit backtest_started();
    } else {
        qDebug() << "[Backtester] Backtest is already running";
    }
}

void Backtester::stop_backtest() {
    log("stop_backtest called");
    if (backtest_timer_->isActive()) {
        backtest_timer_->stop();
    }
    running_ = false;
}

void Backtester::update_gui() {
    int32_t bid = book_.get_best_bid_price();
    int32_t ask = book_.get_best_ask_price();
    std::string time_string = book_.get_formatted_time_fast();

    QDateTime date_time = QDateTime::fromString(QString::fromStdString(time_string), "yyyy-MM-dd HH:mm:ss.zzz");
    qint64 timestamp = date_time.toMSecsSinceEpoch();

    emit update_chart(timestamp, bid, ask);
}

void Backtester::run_backtest() {
    log(QString("run_backtest started on thread: %1").arg((quint64) QThread::currentThreadId()));

    if (current_message_index_ == 0) {
        log("Starting backtest from the beginning...");
    } else {
        log(QString("Resuming backtest from message index: %1").arg(current_message_index_));
    }

    running_ = true;
    first_update_ = false;

    QElapsedTimer total_timer;
    total_timer.start();

    QElapsedTimer update_timer;
    update_timer.start();

    while (running_ && current_message_index_ < messages_.size()) {
        if (current_message_index_ % 10000 == 0) {
            log(QString("Processing message %1 of %2")
                        .arg(current_message_index_)
                        .arg(messages_.size()));
        }

        const auto &msg = messages_[current_message_index_];
        book_.process_msg(msg);

        std::string curr_time = book_.get_formatted_time_fast();

        if (curr_time >= start_time_ && update_timer.elapsed() >= 16) {
            update_gui();
            update_timer.restart();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 16);
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

                while (!strategy->trade_queue_.empty()) {
                    auto [is_buy, price, size] = strategy->trade_queue_.front();
                    strategy->trade_queue_.pop();
                    emit trade_executed(QString::fromStdString(curr_time), is_buy, price, size);
                }
            }
        }

        if (current_message_index_ % 10000 == 0) {
            int progress = static_cast<int>((static_cast<double>(current_message_index_) / messages_.size()) * 100);
            emit update_progress(progress);
            log(QString("Processed %1 messages, Progress: %2%")
                        .arg(current_message_index_)
                        .arg(progress));
        }

        if (curr_time >= end_time_) {
            log("Reached end time, stopping backtest");
            break;
        }

        if (current_message_index_ == messages_.size() - 1) {
            break;
        }

        ++current_message_index_;
    }

    log(QString("run_backtest finished. Processed %1 messages in %2 seconds")
                .arg(current_message_index_)
                .arg(total_timer.elapsed() / 1000.0));

    emit update_progress(100);

    for (const auto &strategy: strategies_) {
        log(QString("Strategy PNL: %1, Final position: %2")
                    .arg(strategy->get_pnl())
                    .arg(strategy->get_position()));
    }

    running_ = false;
    emit backtest_finished();
    stop_backtest();
}
