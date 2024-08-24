#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <map>
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
    std::unique_ptr<WebSocketServer> ws_server;
    std::vector<std::string> batch_updates;
    const size_t BATCH_SIZE = 20;
    std::thread ws_thread;


public:
    DatabaseManager(const std::string &host, int port) : tcp_host(host), tcp_port(port) {
        ws_server = std::make_unique<WebSocketServer>();
        ws_thread = std::thread([this]() {
            this->ws_server->run(9002);
        });
    }


    ~ DatabaseManager() {
        if (ws_server) {
            ws_server->stop();
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }
    }

    void send_batch_to_database() {
        if (batch_updates.empty()) return;

        std::stringstream combined_update;
        for (const auto& update : batch_updates) {
            combined_update << update;
        }
        send_line_protocol_tcp(combined_update.str());
        batch_updates.clear();
    }

    void send_line_protocol_tcp(const std::string &line_protocol) {
        std::cout << "attempting to send line protocol: [" << line_protocol << "]" << std::endl;

        int sock = 0;
        struct sockaddr_in serv_addr;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "socket creation error: " << strerror(errno) << std::endl;
            return;
        }

        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            std::cerr << "error setting socket options: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(tcp_port);

        const char *host_cstr = tcp_host.c_str();
        if (inet_pton(AF_INET, host_cstr, &serv_addr.sin_addr) <= 0) {
            std::cerr << "invalid address: " << tcp_host << " Error: "
                      << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "connection failed to host: " << tcp_host << " on port: " << tcp_port << " Error: "
                      << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        ssize_t sent_bytes = send(sock, line_protocol.c_str(), line_protocol.length(), 0);
        if (sent_bytes < 0) {
            std::cerr << "error sending data: " << strerror(errno) << std::endl;
        } else {
            std::cout << "successfully sent data: " << line_protocol << std::endl;
        }

        close(sock);
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
            for (const auto &[price, limit_ptr] : side) {
                if (++count > 20) break;

                nlohmann::json level = {
                        {"price", price},
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

        ws_server->broadcast(orderbook_update.dump());

        for (const auto &update : db_updates) {
            batch_updates.push_back(update);
            if (batch_updates.size() >= BATCH_SIZE) {
                send_batch_to_database();
            }
        }
        if (!batch_updates.empty()) {
            send_batch_to_database();
        }
    }

};