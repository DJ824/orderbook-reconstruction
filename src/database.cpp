#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <map>
#include <thread>
#include <fstream>
#include <utility>
#include <vector>
#include <queue>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <nlohmann/json.hpp>
#include "limit.h"

class DatabaseManager {
private:
    std::string tcp_host_;
    int tcp_port_;
    int sock_;
    struct sockaddr_in serv_addr_;
    std::thread db_thread_;
    std::thread orderbook_thread_;
    std::queue<std::string> db_queue_;
    std::mutex db_mutex_;
    std::condition_variable db_cv_;
    std::atomic<bool> stop_threads_{false};

    struct OrderBookUpdate {
        std::vector<std::pair<int32_t, uint64_t>> bids;
        std::vector<std::pair<int32_t, uint64_t>> offers;
        uint64_t timestamp;
    };
    std::queue<OrderBookUpdate> orderbook_queue_;
    std::mutex orderbook_mutex_;
    std::condition_variable orderbook_cv_;

public:
    DatabaseManager(std::string host, int port) : tcp_host_(std::move(host)), tcp_port_(port), sock_(-1) {
        memset(&serv_addr_, 0, sizeof(serv_addr_));
        serv_addr_.sin_family = AF_INET;
        serv_addr_.sin_port = htons(tcp_port_);
        if (inet_pton(AF_INET, tcp_host_.c_str(), &serv_addr_.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << tcp_host_ << std::endl;
        }

        db_thread_ = std::thread(&DatabaseManager::process_database_queue, this);
        orderbook_thread_ = std::thread(&DatabaseManager::process_orderbook_queue, this);
    }

    ~DatabaseManager() {
        stop_threads_ = true;
        db_cv_.notify_all();

        if (db_thread_.joinable()) {
            db_thread_.join();
        }
        if (sock_ != -1) {
            close(sock_);
        }
    }

    bool connect_to_server() {
        if (sock_ == -1) {
            sock_ = socket(AF_INET, SOCK_STREAM, 0);
            if (sock_ < 0) {
                std::cerr << "socket creation error: " << strerror(errno) << std::endl;
                return false;
            }

            int opt = 1;
            if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
                std::cerr << "error setting socket options: " << strerror(errno) << std::endl;
                close(sock_);
                sock_ = -1;
                return false;
            }

            if (connect(sock_, (struct sockaddr *) &serv_addr_, sizeof(serv_addr_)) < 0) {
                std::cerr << "connection failed to host: " << tcp_host_ << " on port: " << tcp_port_ << " error: "
                          << strerror(errno) << std::endl;
                close(sock_);
                sock_ = -1;
                return false;
            }

            std::cout << "connected to server: " << tcp_host_ << " on port: " << tcp_port_ << std::endl;
        }

        return true;
    }

    void send_line_protocol_tcp(const std::string &line_protocol) {
        {
            std::lock_guard<std::mutex> lock(db_mutex_);
            db_queue_.push(line_protocol);
        }
        db_cv_.notify_one();
    }

    void process_database_queue() {
        while (!stop_threads_) {
            std::unique_lock<std::mutex> lock(db_mutex_);
            db_cv_.wait(lock, [this] { return !db_queue_.empty() || stop_threads_; });

            while (!db_queue_.empty()) {
                std::string line_protocol = db_queue_.front();
                db_queue_.pop();
                lock.unlock();

                if (connect_to_server()) {
                    ssize_t sent_bytes = send(sock_, line_protocol.c_str(), line_protocol.length(), 0);
                    if (sent_bytes < 0) {
                        std::cerr << "error sending data: " << strerror(errno) << std::endl;
                        close(sock_);
                        sock_ = -1;
                    }
                }
                lock.lock();
            }
        }
    }
    /*
    void add_order(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time) {
        std::stringstream line_protocol;
        line_protocol << "orders price=" << price
                      << ",size=" << size << "i"
                      << ",side=" << (side ? "true" : "false")
                      << ",id=" << id << "i"
                      << ",status=\"active\""
                      << " " << unix_time << "\n";
        send_line_protocol_tcp(line_protocol.str());
    }

    void remove_order(uint64_t id) {
        uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        std::stringstream line_protocol;
        line_protocol << "orders id=" << id << "i"
                      << ",status=\"cancelled\""
                      << " " << current_time << "\n";
        send_line_protocol_tcp(line_protocol.str());
    }

    void modify_order(uint64_t id, float new_price, uint32_t new_size, uint64_t unix_time) {
        std::stringstream line_protocol;
        line_protocol << "orders price=" << new_price
                      << ",size=" << new_size << "i"
                      << ",id=" << id << "i"
                      << ",status=\"active\""
                      << " " << unix_time << "\n";
        send_line_protocol_tcp(line_protocol.str());
    }
    */

    template<typename BidCompare, typename AskCompare>
    void update_limit_orderbook(const std::map<int32_t, Limit *, BidCompare> &bids,
                                const std::map<int32_t, Limit *, AskCompare> &offers) {
        OrderBookUpdate update;
        update.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        auto process_side = [](const auto &side, std::vector<std::pair<int32_t, uint64_t>> &update_side) {
            int count = 0;
            for (const auto &[price, limit_ptr] : side) {
                if (++count > 20) break;
                update_side.emplace_back(price, limit_ptr->volume_);
            }
        };

        process_side(bids, update.bids);
        process_side(offers, update.offers);

        {
            std::lock_guard<std::mutex> lock(orderbook_mutex_);
            orderbook_queue_.push(std::move(update));
        }
        orderbook_cv_.notify_one();
    }

    void process_orderbook_queue() {
        while (!stop_threads_) {
            std::unique_lock<std::mutex> lock(orderbook_mutex_);
            orderbook_cv_.wait(lock, [this] { return !orderbook_queue_.empty() || stop_threads_; });

            while (!orderbook_queue_.empty()) {
                auto update = std::move(orderbook_queue_.front());
                orderbook_queue_.pop();
                lock.unlock();

                std::vector<std::string> db_updates;

                auto format_data = [&](const auto &side, bool is_bid) {
                    for (const auto &[price, volume] : side) {
                        std::stringstream line_protocol;
                        line_protocol << "limit_orderbook "
                                      << "price=" << price << "i,"
                                      << "volume=" << static_cast<long>(volume) << "i,"
                                      << "side=" << (is_bid ? "true" : "false")
                                      << " " << update.timestamp;
                        db_updates.push_back(line_protocol.str());
                    }
                };

                format_data(update.bids, true);
                format_data(update.offers, false);

                for (const auto &update_str : db_updates) {
                    send_line_protocol_tcp(update_str + "\n");
                }

                lock.lock();
            }
        }
    }
};