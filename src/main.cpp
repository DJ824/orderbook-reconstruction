#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QObject>
#include <QTime>
#include "backtester.h"
#include "parser.h"
#include "database.h"
#include "orderbook.h"
#include "book_gui.h"
#include "strategies/imbalance_strat.cpp"
#include <chrono>
#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    try {
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Main] Initializing backtester...";

        DatabaseManager db_manager("127.0.0.1", 9009);



        Orderbook book(db_manager);

        auto parsing_start = std::chrono::high_resolution_clock::now();
        auto parser = std::make_unique<Parser>("es0604.csv");
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Main] Parsing messages...";
        parser->parse();
        auto parsing_end = std::chrono::high_resolution_clock::now();
        auto parsing_duration = std::chrono::duration_cast<std::chrono::duration<double>>(parsing_end - parsing_start);

        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Main] Parsing completed in" << parsing_duration.count() << "seconds";

        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Main] Setting up backtester and strategies...";

        BookGui *gui = new BookGui();
        gui->show();

        Backtester *backtester = new Backtester(db_manager, parser->message_stream_);
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                 << "[Main] Backtester created on thread:" << QThread::currentThreadId();
        backtester->add_strategy(std::make_unique<ImbalanceStrat>(db_manager));

        qDebug() << "Connection established (start_backtest):"
                 << QObject::connect(gui, &BookGui::start_backtest, backtester, &Backtester::handleStartSignal, Qt::QueuedConnection);
        qDebug() << "Connection established (stop_backtest):"
                 << QObject::connect(gui, &BookGui::stop_backtest, backtester, &Backtester::stop_backtest, Qt::QueuedConnection);
        qDebug() << "Connection established (backtest_finished):"
                 << QObject::connect(backtester, &Backtester::backtest_finished, gui, &BookGui::on_backtest_finished, Qt::QueuedConnection);
        qDebug() << "Connection established (update_progress):"
                 << QObject::connect(backtester, &Backtester::update_progress, gui, &BookGui::update_progress, Qt::QueuedConnection);
        qDebug() << "Connection established (trade_executed):"
                 << QObject::connect(backtester, &Backtester::trade_executed, gui, &BookGui::log_trade, Qt::QueuedConnection);
        qDebug() << "Connection established (update_chart):"
                 << QObject::connect(backtester, &Backtester::update_chart, gui, &BookGui::add_data_point, Qt::QueuedConnection);
        qDebug() << "Connection established (backtest_error):"
                 << QObject::connect(backtester, &Backtester::backtest_error, gui, &BookGui::on_backtest_error, Qt::QueuedConnection);
        qDebug() << "Connection established (update_orderbook_stats):"
                 << QObject::connect(backtester, &Backtester::update_orderbook_stats, gui, &BookGui::update_orderbook_stats, Qt::QueuedConnection);

        qDebug() << "Connection established (restart_backtest):"
                 << QObject::connect(gui, &BookGui::restart_backtest, backtester, &Backtester::restart_backtest, Qt::QueuedConnection);

        QObject::connect(backtester, &Backtester::backtest_finished, []() {
            qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")
                     << "[Main] Backtesting completed";
        });

        return app.exec();
    } catch (const std::exception &e) {
        qCritical() << QTime::currentTime().toString("hh:mm:ss.zzz")
                    << "[Main] An error occurred:" << e.what();
        return 1;
    }
}