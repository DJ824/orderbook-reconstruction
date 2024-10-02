#pragma once
#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include "strategy.h"
#include "orderbook.h"
#include "database.h"
#include "message.h"
#include <vector>
#include <memory>
#include <atomic>

class Backtester : public QObject {
Q_OBJECT

public:
    explicit Backtester(Orderbook& book, DatabaseManager& db_manager,
                        const std::vector<message>& messages, QObject* parent = nullptr);
    ~Backtester() override;

    void add_strategy(std::unique_ptr<Strategy> strategy);

public slots:
    void start_backtest();
    void stop_backtest();
    void handleStartSignal();


private slots:
    void run_backtest();

signals:
    void backtest_started();
    void backtest_finished();
    void update_progress(int progress);
    void trade_executed(const QString& timestamp, bool is_buy, int32_t price, int size);
    void update_chart(qint64 timestamp, double bestBid, double bestAsk);
    void backtest_error(const QString& error_message);

private:
    QTimer *backtest_timer_;

    Orderbook &book_;
    std::vector<std::unique_ptr<Strategy>> strategies_;
    DatabaseManager& db_manager_;
    bool first_update_;
    size_t current_message_index_;
    const int UPDATE_INTERVAL = 1000;
    std::atomic<bool> running_;
    std::vector<message> messages_;
    const std::string start_time_ = "2024-05-28 09:30:00.000";
    const std::string end_time_ = "2024-05-28 16:00:00.000";
    QThread worker_thread_;

    void update_gui();
    void process_message(const message& msg);

    void log(const QString& message) {
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Backtester]" << message;
    }
};