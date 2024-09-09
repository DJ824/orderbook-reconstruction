

#include "strategy.h"
#include <vector>

class Backtester {
private:
    Orderbook &book_;
    std::vector<std::unique_ptr<Strategy>> strategies_;
    int count_ = 0;
    DatabaseManager& db_manager_;
    bool first_update = false;

public:
    Backtester(Orderbook& book, DatabaseManager& db_manager)
            : book_(book), db_manager_(db_manager), count_(0) {}

    void add_strategy(std::unique_ptr<Strategy> strategy) {
        strategies_.push_back(std::move(strategy));
    }

    void run(const std::vector<message> &messages) {
        const std::string start_time = "2024-05-24 09:30:00.000";
        const std::string end_time = "2024-05-24 16:00:00.000";
        std::string prev_time;

        for (const auto &msg : messages) {
            ++count_;
            book_.process_msg(msg);

            std::string curr_time = book_.get_formatted_time_fast();
            if (count_ < 14000 && !first_update) {
                book_.calculate_vols();
                first_update = true;
            }

            if (count_ % 1000 == 0) {
                book_.calculate_vols();
            }

            if (count_ % 100 == 0) {
                db_manager_.update_limit_orderbook(book_.bids_, book_.offers_);

            }
            if (curr_time >= start_time) {
                //book_.calculate_vols();
                book_.calculate_imbalance();
                //std::cout << book_.get_formatted_time() << std::endl;
                int diff = std::strcmp(curr_time.c_str() + 17, prev_time.c_str() + 17);
                if (diff >= 2) {
                    for (auto& strategy : strategies_) {
                        strategy->on_book_update(book_);
                    }
                }
            }
            prev_time = curr_time;

            if (curr_time >= end_time) {
                break;
            }

        }
    }
};




