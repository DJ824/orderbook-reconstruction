
#include <chrono>
#include "../include/orderbook.h"
#include "../include/parser.h"

int main() {
    orderbook book;
    parser p("modified_esm4_mbo.csv");
    p.parse();
    auto start = std::chrono::high_resolution_clock::now();
    int add_messages = 0;
    int remove_messages = 0;
    int modify_messages = 0;
    int trade_messages = 0;
    int count = 0;

    for (const auto &msg: p.message_stream) {
        ++count;
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

}
