#include <chrono>
#include <iostream>
#include <memory>
#include "../include/parser.h"
#include "orderbook.h"
#include "backtester.cpp"
#include "strategies/imbalance_strat.cpp"
#include "strategies/skew_strat.cpp"

int main() {
    try {
        DatabaseManager db_manager("127.0.0.1", 9009);
        Orderbook book(db_manager);

        auto parsing_start = std::chrono::high_resolution_clock::now();
        auto parser = std::make_unique<Parser>("es0528.csv");
        std::cout << "parsing messages... " << std::endl;
        parser->parse();
        auto parsing_end = std::chrono::high_resolution_clock::now();
        auto parsing_duration = std::chrono::duration_cast<std::chrono::duration<double>>(parsing_end - parsing_start);
        std::cout << "parsing completed in " << parsing_duration.count() << " seconds" << std::endl;

        std::cout << "processing into book" << std::endl;
        auto strategy = std::make_unique<ImbalanceStrat>(db_manager);

        Backtester backtester(book, db_manager);
        backtester.add_strategy(std::move(strategy));

        auto backtesting_start = std::chrono::high_resolution_clock::now();
        backtester.run(parser->message_stream_);
        auto backtesting_end = std::chrono::high_resolution_clock::now();
        auto backtesting_duration = std::chrono::duration_cast<std::chrono::duration<double>>(backtesting_end - backtesting_start);

        std::cout << "backtesting completed in " << backtesting_duration.count() << " seconds" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}