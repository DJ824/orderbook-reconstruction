#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <map>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <nlohmann/json.hpp>
#include "limit.h"
#include "lock_free_queue.h"

class DatabaseManager {
private:
    std::string tcp_host_;
    int tcp_port_;
    int sock_;
    struct sockaddr_in serv_addr_;
    std::thread db_thread_;
    std::thread orderbook_thread_;
    std::unique_ptr<LockFreeQueue<std::string, 1000000>> db_queue_;
    std::atomic<bool> stop_threads_{false};

    struct OrderBookUpdate {
        std::vector<std::pair<int32_t, uint64_t>> bids_;
        std::vector<std::pair<int32_t, uint64_t>> offers_;
        uint64_t timestamp_;
    };
    std::unique_ptr<LockFreeQueue<OrderBookUpdate, 1000000>> orderbook_queue_;

public:
    DatabaseManager(std::string host, int port)
            : tcp_host_(std::move(host)), tcp_port_(port), sock_(-1),
              db_queue_(std::make_unique<LockFreeQueue<std::string, 1000000>>()),
              orderbook_queue_(std::make_unique<LockFreeQueue<OrderBookUpdate, 1000000>>())
    {
        memset(&serv_addr_, 0, sizeof(serv_addr_));
        serv_addr_.sin_family = AF_INET;
        serv_addr_.sin_port = htons(tcp_port_);
        if (inet_pton(AF_INET, tcp_host_.c_str(), &serv_addr_.sin_addr) <= 0) {
            std::cerr << "invalid address: " << tcp_host_ << std::endl;
        }

        db_thread_ = std::thread(&DatabaseManager::process_database_queue, this);
        orderbook_thread_ = std::thread(&DatabaseManager::process_orderbook_queue, this);
    }

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    DatabaseManager(DatabaseManager&& other) noexcept
            : tcp_host_(std::move(other.tcp_host_)),
              tcp_port_(other.tcp_port_),
              sock_(other.sock_),
              serv_addr_(other.serv_addr_),
              db_queue_(std::move(other.db_queue_)),
              orderbook_queue_(std::move(other.orderbook_queue_)),
              stop_threads_(other.stop_threads_.load())
    {
        other.sock_ = -1;
        if (other.db_thread_.joinable()) {
            other.db_thread_.join();
        }
        if (other.orderbook_thread_.joinable()) {
            other.orderbook_thread_.join();
        }
        db_thread_ = std::thread(&DatabaseManager::process_database_queue, this);
        orderbook_thread_ = std::thread(&DatabaseManager::process_orderbook_queue, this);
    }

    DatabaseManager& operator=(DatabaseManager&& other) noexcept {
        if (this != &other) {
            stop_threads_ = true;
            if (db_thread_.joinable()) db_thread_.join();
            if (orderbook_thread_.joinable()) orderbook_thread_.join();
            if (sock_ != -1) close(sock_);

            tcp_host_ = std::move(other.tcp_host_);
            tcp_port_ = other.tcp_port_;
            sock_ = other.sock_;
            serv_addr_ = other.serv_addr_;
            db_queue_ = std::move(other.db_queue_);
            orderbook_queue_ = std::move(other.orderbook_queue_);
            stop_threads_ = other.stop_threads_.load();

            other.sock_ = -1;
            if (other.db_thread_.joinable()) other.db_thread_.join();
            if (other.orderbook_thread_.joinable()) other.orderbook_thread_.join();

            db_thread_ = std::thread(&DatabaseManager::process_database_queue, this);
            orderbook_thread_ = std::thread(&DatabaseManager::process_orderbook_queue, this);
        }
        return *this;
    }

    ~DatabaseManager() {
        stop_threads_ = true;
        if (db_thread_.joinable()) db_thread_.join();
        if (orderbook_thread_.joinable()) orderbook_thread_.join();
        if (sock_ != -1) {
            close(sock_);
            sock_ = -1;
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
        db_queue_->enqueue(line_protocol);
    }

    void process_database_queue() {
        while (!stop_threads_ || !db_queue_->empty()) {
            std::optional<std::string> line_protocol = db_queue_->dequeue();
            if (line_protocol) {
                if (connect_to_server()) {
                    ssize_t sent_bytes = send(sock_, line_protocol->c_str(), line_protocol->length(), 0);
                    if (sent_bytes < 0) {
                        std::cerr << "error sending data: " << strerror(errno) << std::endl;
                        close(sock_);
                        sock_ = -1;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    template<typename BidCompare, typename AskCompare>
    inline void update_limit_orderbook(const std::map<int32_t, Limit *, BidCompare> &bids,
                                       const std::map<int32_t, Limit *, AskCompare> &offers) {
        OrderBookUpdate update;
        update.timestamp_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        auto process_side = [](const auto &side, std::vector<std::pair<int32_t, uint64_t>> &update_side) {
            int count = 0;
            for (const auto &[price, limit_ptr] : side) {
                if (++count > 20) break;
                update_side.emplace_back(price, limit_ptr->volume_);
            }
        };

        process_side(bids, update.bids_);
        process_side(offers, update.offers_);

        orderbook_queue_->enqueue(std::move(update));
    }

    inline void process_orderbook_queue() {
        while (!stop_threads_ || !orderbook_queue_->empty()) {
            std::optional<OrderBookUpdate> update = orderbook_queue_->dequeue();
            if (update) {
                std::vector<std::string> db_updates;

                auto format_data = [&](const auto &side, bool is_bid) {
                    for (const auto &[price, volume] : side) {
                        std::stringstream line_protocol;
                        line_protocol << "limit_orderbook "
                                      << "price=" << price << "i,"
                                      << "volume=" << static_cast<long>(volume) << "i,"
                                      << "side=" << (is_bid ? "true" : "false")
                                      << " " << update->timestamp_;
                        db_updates.push_back(line_protocol.str());
                    }
                };

                format_data(update->bids_, true);
                format_data(update->offers_, false);

                for (const auto &update_str : db_updates) {
                    send_line_protocol_tcp(update_str + "\n");
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
};