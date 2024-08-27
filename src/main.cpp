#include <chrono>
#include <iostream>
#include "../include/parser.h"
#include "orderbook.h"

int main() {

    //const char* conn_str = "hostaddr=127.0.0.1 port=8812 dbname=qdb user=admin password=quest";
    const char* api_url = "http://localhost:9009/imp";

    //DatabaseManager db_manager(conn_str);
    DatabaseManager db_manager("127.0.0.1", 9009);

    orderbook book(db_manager);
    //db_manager.create_tables();
    //db_manager.add_status_column();

    Parser p("modified_esm4_mbo.csv");
    p.parse();

    auto start = std::chrono::high_resolution_clock::now();

    int add_messages = 0;
    int remove_messages = 0;
    int modify_messages = 0;
    int trade_messages = 0;
    int count = 0;

    for (const auto &msg: p.message_stream_) {
        ++count;
        /*
        if (count == 300000) {
            break;
        }
         */

        switch (msg.action_) {
            case 'A':
                book.add_limit_order(msg.id_, msg.price_, msg.size_, msg.side_, msg.time_);
                ++add_messages;
                break;
            case 'C':
                book.remove_order(msg.id_, msg.price_, msg.size_, msg.side_);
                ++remove_messages;
                break;
            case 'M':
                book.modify_order(msg.id_, msg.price_, msg.size_, msg.side_, msg.time_);
                ++modify_messages;
                break;
            case 'T':
                book.trade_order(msg.id_, msg.price_, msg.size_, msg.side_);
                ++trade_messages;
                break;
        }
        db_manager.update_limit_orderbook(book.bids_, book.offers_);

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

    return 0;




}