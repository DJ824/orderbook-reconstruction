#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <map>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <nlohmann/json.hpp>
#include "limit.h"
#include "websocket.cpp"

class DatabaseManager {
private:
    std::string tcp_host;
    int tcp_port;
    int sock;
    struct sockaddr_in serv_addr;
    std::unique_ptr<WebSocketServer> ws_server;
    std::thread db_thread;
    std::thread ws_thread;
    std::queue<std::string> db_queue;
    std::queue<std::string> ws_queue;
    std::mutex db_mutex;
    std::mutex ws_mutex;
    std::condition_variable db_cv;
    std::condition_variable ws_cv;
    bool stop_threads = false;

public:
    DatabaseManager(const std::string &host, int port) : tcp_host(host), tcp_port(port), sock(-1) {
        ws_server = std::make_unique<WebSocketServer>();

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(tcp_port);
        if (inet_pton(AF_INET, tcp_host.c_str(), &serv_addr.sin_addr) <= 0) {
            std::cerr << "invalid address: " << tcp_host << std::endl;
        }

        db_thread = std::thread(&DatabaseManager::processDatabaseQueue, this);

        ws_thread = std::thread(&DatabaseManager::processWebSocketQueue, this);

        std::thread ws_server_thread([this]() {
            ws_server->run(9002);
        });
        ws_server_thread.detach();
    }

    ~DatabaseManager() {
        stop_threads = true;
        db_cv.notify_all();
        ws_cv.notify_all();

        if (db_thread.joinable()) {
            db_thread.join();
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }
        if (sock != -1) {
            close(sock);
        }
        ws_server->stop();
    }

    bool connect_to_server() {
        if (sock == -1) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                std::cerr << "socket creation error: " << strerror(errno) << std::endl;
                return false;
            }

            int opt = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
                std::cerr << "error setting socket options: " << strerror(errno) << std::endl;
                close(sock);
                sock = -1;
                return false;
            }

            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                std::cerr << "connection failed to host: " << tcp_host << " on port: " << tcp_port << " error: "
                          << strerror(errno) << std::endl;
                close(sock);
                sock = -1;
                return false;
            }

            std::cout << "connected to server: " << tcp_host << " on port: " << tcp_port << std::endl;
        }

        return true;
    }

    void send_line_protocol_tcp(const std::string &line_protocol) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            db_queue.push(line_protocol);
        }
        db_cv.notify_one();
    }

    void broadcast_to_websocket(const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(ws_mutex);
            ws_queue.push(message);
        }
        ws_cv.notify_one();
    }

    void processDatabaseQueue() {
        while (!stop_threads) {
            std::unique_lock<std::mutex> lock(db_mutex);
            db_cv.wait(lock, [this] { return !db_queue.empty() || stop_threads; });

            while (!db_queue.empty()) {
                std::string line_protocol = db_queue.front();
                db_queue.pop();
                lock.unlock();

                if (connect_to_server()) {
                    ssize_t sent_bytes = send(sock, line_protocol.c_str(), line_protocol.length(), 0);
                    if (sent_bytes < 0) {
                        std::cerr << "error sending data: " << strerror(errno) << std::endl;
                        close(sock);
                        sock = -1;
                    } else {
                        std::cout << "successfully sent data to database: " << line_protocol << std::endl;
                    }
                }

                lock.lock();
            }
        }
    }

    void processWebSocketQueue() {
        while (!stop_threads) {
            std::unique_lock<std::mutex> lock(ws_mutex);
            ws_cv.wait(lock, [this] { return !ws_queue.empty() || stop_threads; });

            while (!ws_queue.empty()) {
                std::string message = ws_queue.front();
                ws_queue.pop();
                lock.unlock();

                ws_server->broadcast(message);
                std::cout << "broadcast message to websocket clients: " << message << std::endl;

                lock.lock();
            }
        }
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

    template<typename BidCompare, typename AskCompare>
    void update_limit_orderbook(const std::map<float, limit *, BidCompare> &bids,
                                const std::map<float, limit *, AskCompare> &offers) {
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
