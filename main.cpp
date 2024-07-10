#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include "orderbook.h"

int main() {
    orderbook book;
    std::ifstream file("modified_esm4_mbo.csv");
    std::string line;

    if (!std::getline(file, line)) {
        std::cerr << "Error reading the first line or file is empty." << std::endl;
        return 1;
    }

    int line_count = 0;
    const int max_lines = 4000000;
    auto start = std::chrono::high_resolution_clock::now();
    int add_messages = 0;
    int remove_messages = 0;
    int modify_messages = 0;
    int trade_messages = 0;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;

        std::getline(ss, token, ',');
        uint64_t ts_event, order_id;
        float price;
        uint32_t size;
        std::string action, side;

        std::getline(ss, token, ',');
        ts_event = std::stoull(token);

        for (int i = 0; i < 3; ++i)
            std::getline(ss, token, ',');

        std::getline(ss, action, ',');
        std::getline(ss, side, ',');
        std::getline(ss, token, ',');
        price = std::stof(token);
        std::getline(ss, token, ',');
        size = std::stoul(token);

        for (int i = 0; i < 1; ++i)
            std::getline(ss, token, ',');
        std::getline(ss, token, ',');
        order_id = std::stoull(token);

        bool bid_or_ask = (side == "B");

        ++line_count;
        if (action == "A") {
            book.add_limit_order(order_id, price, size, bid_or_ask, ts_event);
            ++add_messages;
        }

        if (action == "C") {
            book.remove_order(order_id, price, size, bid_or_ask);
            ++remove_messages;
        }

        if (action == "M") {
            book.modify_order(order_id, price, size, bid_or_ask, ts_event);
            ++modify_messages;
        }

        if (action == "T") {
            book.trade_order(order_id, price, size, bid_or_ask);
            ++trade_messages;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
    std::cout << "seconds: " << seconds.count() << std::endl;
    std::cout << "current book count: " << book.get_count() << std::endl;
    std::cout << "number of add messages processed: " << add_messages << std::endl;
    std::cout << "number of remove messages processed: " << remove_messages << std::endl;
    std::cout << "number of modify messages processed: " << modify_messages << std::endl;
    std::cout << "number of trade messages processed: " << trade_messages << std::endl;
    std::cout << "total number of messages processed: "
              << modify_messages + add_messages + remove_messages + trade_messages << std::endl;
    file.close();
    return 0;
}
