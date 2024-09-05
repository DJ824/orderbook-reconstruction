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
#include "websocket.cpp"
#pragma once

class DatabaseManager {
private:
    std::string tcp_host_;
    int tcp_port_;
    int sock_;
    struct sockaddr_in serv_addr_;
    std::unique_ptr<WebSocketServer> ws_server_;
    std::thread db_thread_;
    std::thread ws_thread_;
    std::queue<std::string> db_queue_;
    std::queue<std::string> ws_queue_;
    std::mutex db_mutex_;
    std::mutex ws_mutex_;
    std::condition_variable db_cv_;
    std::condition_variable ws_cv_;
    bool stop_threads_ = false;

    void send_batch_to_database(const std::vector<std::string>& line_protocols) {
        std::string batch = std::accumulate(line_protocols.begin(), line_protocols.end(), std::string(),
                                            [](const std::string& a, const std::string& b) {
                                                return a + (a.empty() ? "" : "\n") + b;
                                            });
        //std::cout << batch << std::endl;

        send_line_protocol_tcp(batch);
    }

public:
    DatabaseManager(std::string host, int port) : tcp_host_(std::move(host)), tcp_port_(port), sock_(-1) {
        ws_server_ = std::make_unique<WebSocketServer>();

        memset(&serv_addr_, 0, sizeof(serv_addr_));
        serv_addr_.sin_family = AF_INET;
        serv_addr_.sin_port = htons(tcp_port_);
        if (inet_pton(AF_INET, tcp_host_.c_str(), &serv_addr_.sin_addr) <= 0) {
            std::cerr << "invalid address: " << tcp_host_ << std::endl;
        }

        db_thread_ = std::thread(&DatabaseManager::process_database_queue, this);

        ws_thread_ = std::thread(&DatabaseManager::process_websocket_queue, this);

        std::thread ws_server_thread([this]() {
            ws_server_->run(9002);
        });
        ws_server_thread.detach();
    }

    ~DatabaseManager() {
        stop_threads_ = true;
        db_cv_.notify_all();
        ws_cv_.notify_all();

        if (db_thread_.joinable()) {
            db_thread_.join();
        }
        if (ws_thread_.joinable()) {
            ws_thread_.join();
        }
        if (sock_ != -1) {
            close(sock_);
        }
        ws_server_->stop();
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

    void broadcast_to_websocket(const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(ws_mutex_);
            ws_queue_.push(message);
        }
        ws_cv_.notify_one();
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
                    } else {
                        //   std::cout << "successfully sent data to database: " << line_protocol << std::endl;
                    }
                }
                lock.lock();
            }
        }
    }

    void process_websocket_queue() {
        while (!stop_threads_) {
            std::unique_lock<std::mutex> lock(ws_mutex_);
            ws_cv_.wait(lock, [this] { return !ws_queue_.empty() || stop_threads_; });

            while (!ws_queue_.empty()) {
                std::string message = ws_queue_.front();
                ws_queue_.pop();
                lock.unlock();

                ws_server_->broadcast(message);
                //std::cout << "broadcast message to websocket clients: " << message << std::endl;

                lock.lock();
            }
        }
    }

    void send_csv_to_database(const std::string& csv_file_path, const std::string& measurement_name) {
        std::ifstream file(csv_file_path);
        if (!file.is_open()) {
            std::cerr << "error opening file: " << csv_file_path << std::endl;
            return;
        }

        std::string line;
        std::vector<std::string> line_protocols(1001);

        std::getline(file, line);

        while (std::getline(file, line)) {
            std::stringstream line_protocol;
            line_protocol << measurement_name << " " << line;
            line_protocols.push_back(line_protocol.str());

            if (line_protocols.size() >= 1000) {
                send_batch_to_database(line_protocols);
                line_protocols.clear();
            }
        }

        if (!line_protocols.empty()) {
            send_batch_to_database(line_protocols);
        }
        file.close();
    }

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

    void send_market_order_to_gui(float price, uint32_t size, bool side, uint64_t unix_time) {
        nlohmann::json market_order = {
                {"time",  unix_time},
                {"price", price},
                {"size",  size},
                {"side",  side ? "buy" : "sell"}
        };

        broadcast_to_websocket(market_order.dump());
    }

    void send_timestamp_to_gui(const std::string& timestamp) {
        nlohmann::json timestamp_json = {
                {"type", "timestamp"},
                {"value", timestamp}
        };
        broadcast_to_websocket(timestamp_json.dump());
    }

    template<typename BidCompare, typename AskCompare>
    void update_limit_orderbook(const std::map<float, Limit *, BidCompare> &bids,
                                const std::map<float, Limit *, AskCompare> &offers) {
        uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        nlohmann::json orderbook_update;
        orderbook_update["timestamp"] = current_time;
        orderbook_update["bids"] = nlohmann::json::array();
        orderbook_update["asks"] = nlohmann::json::array();

        std::vector<std::string> db_updates;

        auto process_side = [&](const auto &side, const std::string &side_str, bool is_bid) {
            int count = 0;
            for (const auto &[price, limit_ptr]: side) {
                if (++count > 20) break;

                nlohmann::json level = {
                        {"price",  price},
                        {"volume", limit_ptr->volume_}
                };

                if (is_bid) {
                    orderbook_update["bids"].push_back(level);
                } else {
                    orderbook_update["asks"].push_back(level);
                }

                std::stringstream line_protocol;
                line_protocol << "limit_orderbook "
                              << "price=" << price << ",volume=" << static_cast<long>(limit_ptr->volume_)
                              << "i,side=" << (is_bid ? "true" : "false")
                              << " " << current_time << "\n";
                db_updates.push_back(line_protocol.str());
            }
        };

        process_side(bids, "bids", true);
        process_side(offers, "asks", false);

        std::sort(orderbook_update["asks"].begin(), orderbook_update["asks"].end(),
                  [](const nlohmann::json &a, const nlohmann::json &b) {
                      return a["price"].get<float>() > b["price"].get<float>();
                  });

        broadcast_to_websocket(orderbook_update.dump());

        for (const auto &update: db_updates) {
            send_line_protocol_tcp(update);
        }
    }
};