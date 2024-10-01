#include <QApplication>
#include "backtester.h"
#include "parser.h"
#include "database.h"
#include "orderbook.h"
#include "strategies/imbalance_strat.cpp"
#include <chrono>
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    try {
        std::cout << "Initializing backtester..." << std::endl;
        DatabaseManager db_manager("127.0.0.1", 9009);
        Orderbook book(db_manager);

        auto parsing_start = std::chrono::high_resolution_clock::now();
        auto parser = std::make_unique<Parser>("es0603.csv");
        std::cout << "Parsing messages... " << std::endl;
        parser->parse();
        auto parsing_end = std::chrono::high_resolution_clock::now();
        auto parsing_duration = std::chrono::duration_cast<std::chrono::duration<double>>(parsing_end - parsing_start);
        std::cout << "Parsing completed in " << parsing_duration.count() << " seconds" << std::endl;

        std::cout << "Setting up backtester and strategies..." << std::endl;
        Backtester backtester(book, db_manager, app, parser->message_stream_);
        backtester.add_strategy(std::make_unique<ImbalanceStrat>(db_manager));

        QObject::connect(backtester.chart_view_, &BookGui::start_backtest, &backtester, &Backtester::start_backtest);
        QObject::connect(backtester.chart_view_, &BookGui::stop_backtest, &backtester, &Backtester::stop_backtest);
        QObject::connect(&backtester, &Backtester::backtest_finished, [&]() {
            std::cout << "Backtesting completed" << std::endl;
        });

        return app.exec();
    } catch (const std::exception &e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
}