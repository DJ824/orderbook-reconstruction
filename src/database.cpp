//
// Created by Devang Jaiswal on 8/13/24.
//

#include "database.h"

orderbook_database::orderbook_database(const std::string &dbName, orderbook &book)
        : db_(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE), book_(book) {
    create_tables();
    prepare_statements();
}

void orderbook_database::create_tables() {
    db_.exec("CREATE TABLE IF NOT EXISTS orders ("
             "id INTEGER PRIMARY KEY,"
             "price REAL,"
             "size INTEGER,"
             "side BOOLEAN,"
             "unix_time INTEGER,"
             "limit_price REAL)");

    db_.exec("CREATE TABLE IF NOT EXISTS limits ("
             "price REAL,"
             "side BOOLEAN,"
             "volume INTEGER,"
             "order_count INTEGER,"
             "PRIMARY KEY (price, side))");
}

void orderbook_database::prepare_statements()  {
    insert_order_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "INSERT OR REPLACE INTO orders (id, price, size, side, unix_time, limit_price) "
                                                           "VALUES (?, ?, ?, ?, ?, ?)");

    update_order_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "UPDATE orders SET price = ?, size = ?, unix_time = ?, limit_price = ? "
                                                           "WHERE id = ?");

    delete_order_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "DELETE FROM orders WHERE id = ?");

    insert_limit_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "INSERT OR REPLACE INTO limits (price, side, volume, order_count) "
                                                           "VALUES (?, ?, ?, ?)");

    update_limit_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "UPDATE limits SET volume = ?, order_count = ? "
                                                           "WHERE price = ? AND side = ?");

    delete_limit_stmt_ = std::make_unique<SQLite::Statement>(db_,
                                                           "DELETE FROM limits WHERE price = ? AND side = ?");
}

void orderbook_database::add_limit_order_db(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time) {
    book_.add_limit_order(id, price, size, side, unix_time);

    const order* new_order = book_.order_lookup_[id];
    save_order(new_order);
    save_limit(new_order->parent_);
}

void orderbook_database::remove_order_db(uint64_t id, float price, uint32_t size, bool side) {
    const order* target = book_.order_lookup_[id];
    const limit* target_limit = target->parent_;

    book_.remove_order(id, price, size, side);
    delete_order(id);

    if (target_limit->num_orders_ == 0) {
        delete_limit(price, side);
    } else {
        update_limit(target_limit);
    }
}

void orderbook_database::modify_order_db(uint64_t id, float new_price, uint32_t new_size, bool side,
                                         uint64_t unix_time) {
    const order* target = book_.order_lookup_[id];
    const limit* old_limit = target->parent_;
    float old_price = target->price_;

    book_.modify_order(id, new_price, new_size, side, unix_time);

    const order* updated_order = book_.order_lookup_[id];
    update_order(updated_order);

    if (old_price != new_price) {
        if (old_limit->num_orders_ == 0) {
            delete_limit(old_price, side);
        } else {
            update_limit(old_limit);
        }
        save_limit(updated_order->parent_);
    }
}

void orderbook_database::trade_order_db(uint64_t id, float price, uint32_t size, bool side) {
    std::vector<const limit*> affected_limits;
    if (side) {
        auto it = book_.offers_.lower_bound(price);
        while (it != book_.offers_.end() && size > 0) {
            affected_limits.push_back(it->second);
            size -= it->second->volume_;
            ++it;
        }
    } else {
        auto it = book_.bids_.lower_bound(price);
        while (it != book_.bids_.end() && size > 0) {
            affected_limits.push_back(it->second);
            size -= it->second->volume_;
            ++it;
        }
    }

    book_.trade_order(id, price, size, side);

    for (const auto* limit : affected_limits) {
        if (limit->num_orders_ == 0) {
            delete_limit(limit->price_, !side);
        } else {
            update_limit(limit);
        }
        for (const order* o = limit->head_; o != nullptr; o = o->next_) {
            if (o->filled_) {
                delete_order(o->id_);
            } else update_order(o);
        }
    }
}

void orderbook_database::save_order(const order *order) {
    insert_order_stmt_->bind(1, static_cast<long long>(order->id_));
    insert_order_stmt_->bind(2, order->price_);
    insert_order_stmt_->bind(3, static_cast<int>(order->size));
    insert_order_stmt_->bind(4, order->side_);
    insert_order_stmt_->bind(5, static_cast<long long>(order->unix_time_));
    insert_order_stmt_->bind(6, order->parent_->price_);
    insert_order_stmt_->exec();
    insert_order_stmt_->reset();
}

void orderbook_database::update_order(const order *order) {
    update_order_stmt_->bind(1, order->price_);
    update_order_stmt_->bind(2, static_cast<int>(order->size));
    update_order_stmt_->bind(3, static_cast<long long>(order->unix_time_));
    update_order_stmt_->bind(4, order->parent_->price_);
    update_order_stmt_->bind(5, static_cast<long long>(order->id_));
    update_order_stmt_->exec();
    update_order_stmt_->reset();
}

void orderbook_database::delete_order(uint64_t id) {
    delete_order_stmt_->bind(1, static_cast<long long>(id));
    delete_order_stmt_->exec();
    delete_order_stmt_->reset();
}

void orderbook_database::save_limit(const limit *limit) {
    insert_limit_stmt_->bind(1, limit->price_);
    insert_limit_stmt_->bind(2, limit->side_);
    insert_limit_stmt_->bind(3, static_cast<int>(limit->volume_));
    insert_limit_stmt_->bind(4, static_cast<int>(limit->num_orders_));
    insert_limit_stmt_->exec();
    insert_limit_stmt_->reset();
}

void orderbook_database::update_limit(const limit *limit) {
    update_limit_stmt_->bind(1, static_cast<int>(limit->volume_));
    update_limit_stmt_->bind(2, static_cast<int>(limit->num_orders_));
    update_limit_stmt_->bind(3, limit->price_);
    update_limit_stmt_->bind(4, limit->side_);
    update_limit_stmt_->exec();
    update_limit_stmt_->reset();
}

void orderbook_database::delete_limit(float price, bool side) {
    delete_limit_stmt_->bind(1, price);
    delete_limit_stmt_->bind(2, side);
    delete_limit_stmt_->exec();
    delete_limit_stmt_->reset();
}

std::vector<order> orderbook_database::load_orders() {
    std::vector<order> orders;
    SQLite::Statement query (db_, "SELECT * FROM orders");
    while (query.executeStep()) {
        order target;
        target.id_ = query.getColumn(0).getInt64();
        target.price_ = query.getColumn(1).getDouble();
        target.size = query.getColumn(2).getInt();
        target.side_ = query.getColumn(3).getInt();
        target.unix_time_ = query.getColumn(4).getInt64();
        orders.push_back(target);
    }
    return orders;
}

std::vector<limit> orderbook_database::load_limits() {
    std::vector<limit> limits;
    SQLite::Statement query(db_, "SELECT * FROM limits");
    while (query.executeStep()) {
        limit target(query.getColumn(0).getDouble());
        target.side_ = query.getColumn(1).getInt();
        target.volume_ = query.getColumn(2).getInt();
        target.num_orders_ = query.getColumn(3).getInt();
        limits.push_back(target);
    }
    return limits;
}



