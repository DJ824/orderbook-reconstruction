
#ifndef DATABENTO_ORDERBOOK_DATABASE_H
#define DATABENTO_ORDERBOOK_DATABASE_H
#include <SQLiteCpp/SQLiteCpp.h>
#include "orderbook.h"
#include <string>
#include <vector>

class orderbook_database {
private:
    SQLite::Database db_;
    orderbook &book_;

    void create_tables();
    void prepare_statements();

    std::unique_ptr<SQLite::Statement> insert_order_stmt_;
    std::unique_ptr<SQLite::Statement> update_order_stmt_;
    std::unique_ptr<SQLite::Statement> delete_order_stmt_;
    std::unique_ptr<SQLite::Statement> insert_limit_stmt_;
    std::unique_ptr<SQLite::Statement> update_limit_stmt_;
    std::unique_ptr<SQLite::Statement> delete_limit_stmt_;

    void save_order(const order *order);
    void update_order(const order *order);
    void delete_order(uint64_t id);
    void save_limit(const limit *limit);
    void update_limit(const limit *limit);
    void delete_limit(float price, bool side);

public:
    orderbook_database(const std::string &dbName, orderbook &book);

    void add_limit_order_db(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time);
    void remove_order_db(uint64_t id, float price, uint32_t size, bool side);
    void modify_order_db(uint64_t id, float new_price, uint32_t new_size, bool side, uint64_t unix_time);
    void trade_order_db(uint64_t id, float price, uint32_t size, bool side);
    std::vector<order> load_orders();
    std::vector<limit> load_limits();
};


#endif //DATABENTO_ORDERBOOK_DATABASE_H
