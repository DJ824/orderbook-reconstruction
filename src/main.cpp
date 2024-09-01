#include <chrono>
#include <iostream>
#include "../include/parser.h"
#include "orderbook.h"
#include "backtester.cpp"
#include "strategies/imbalance_strat.cpp"

int main() {
    DatabaseManager db_manager("127.0.0.1", 9009);

    Orderbook book(db_manager);

    Parser p("modified_esm4_mbo.csv");
    std::cout << "parsing messages... " << std::endl;
    p.parse();
    std::cout << "processing into book" << std::endl;
    auto strategy = std::make_unique<ImbalanceStrat>(db_manager);

    Backtester backtester(book, db_manager);
    backtester.add_strategy(std::move(strategy));

    auto start = std::chrono::high_resolution_clock::now();

    backtester.run(p.message_stream_);

    //db_manager.send_csv_to_database("imbalance_strat_log.csv", "strategy_results");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);

    std::cout << "seconds: " << seconds.count() << std::endl;
    std::cout << "current book count: " << book.get_count() << std::endl;
}