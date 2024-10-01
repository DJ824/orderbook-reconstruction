#pragma once
#include <QObject>
#include "strategy.h"
#include "orderbook.h"
#include "database.h"
#include "message.h"
#include "book_gui.h"
#include <QApplication>
#include <QMainWindow>
#include <QDateTime>
#include <vector>
#include <memory>
#include <atomic>

class Backtester : public QObject {
Q_OBJECT

public:
    explicit Backtester(Orderbook& book, DatabaseManager& db_manager, QApplication& app, const std::vector<message>& messages, QObject* parent = nullptr);
    ~Backtester() override;

    void add_strategy(std::unique_ptr<Strategy> strategy);
    void start_backtest();
    void stop_backtest();
    BookGui* chart_view_;


public slots:
    void run_backtest();

signals:
    void backtest_finished();
    void update_progress(int progress);

private:
    Orderbook &book_;
    std::vector<std::unique_ptr<Strategy>> strategies_;
    DatabaseManager& db_manager_;
    bool first_update_;
    size_t current_message_index_;
    QMainWindow window_;
    const int UPDATE_INTERVAL = 1000;
    std::atomic<bool> running_;
    std::vector<message> messages_;
    const std::string start_time_ = "2024-06-03 09:30:00.000";
    const std::string end_time_ = "2024-06-03 16:00:00.000";

    void update_gui();
    void process_message(const message& msg);
};