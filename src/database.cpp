#include "database.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

DatabaseManager::DatabaseManager(std::string host, int port)
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

DatabaseManager::~DatabaseManager() {
    stop_threads_ = true;
    cv.notify_all();
    if (db_thread_.joinable()) db_thread_.join();
    if (orderbook_thread_.joinable()) orderbook_thread_.join();
    if (sock_ != -1) {
        close(sock_);
        sock_ = -1;
    }
}

DatabaseManager::DatabaseManager(DatabaseManager &&other) noexcept
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

DatabaseManager &DatabaseManager::operator=(DatabaseManager &&other) noexcept {
    if (this != &other) {
        stop_threads_ = true;
        cv.notify_all();
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

bool DatabaseManager::connect_to_server() {
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

void DatabaseManager::send_line_protocol_tcp(const std::string &line_protocol) {
    db_queue_->enqueue(line_protocol);
}

void DatabaseManager::process_database_queue() {
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



void DatabaseManager::process_orderbook_queue() {
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

std::string DatabaseManager::send_query(const std::string& query) {
    if (!connect_to_server()) {
        throw std::runtime_error("Failed to connect to QuestDB");
    }

    if (send(sock_, query.c_str(), query.length(), 0) < 0) {
        throw std::runtime_error("Failed to send query to QuestDB");
    }

    std::string response;
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(sock_, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
    }

    if (bytes_received < 0) {
        throw std::runtime_error("Error receiving response from QuestDB");
    }

    return response;
}

std::vector<message> DatabaseManager::parse_csv_response(const std::string& response) {
    std::vector<message> messages;
    std::istringstream response_stream(response);
    std::string line;

    std::getline(response_stream, line);

    while (std::getline(response_stream, line)) {
        std::istringstream line_stream(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(line_stream, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() == 6) {
            try {
                uint64_t ts_event = std::stoull(fields[0]);
                char action = fields[1][0];
                bool side = (fields[2][0] == 'B');
                int32_t price = std::stoi(fields[3]);
                uint32_t size = std::stoul(fields[4]);
                uint64_t order_id = std::stoull(fields[5]);

                messages.emplace_back(order_id, ts_event, size, price, action, side);
            } catch (const std::exception& e) {
                log_error("Error parsing CSV line: " + line + " - " + e.what());
            }
        } else {
            log_error("Invalid number of fields in CSV line: " + line);
        }
    }

    return messages;
}

std::vector<message> DatabaseManager::read_csv_from_questdb() {
    std::vector<message> all_messages;
    const size_t page_size = 1000000;
    size_t offset = 0;

    try {
        while (true) {
            std::string query = "SELECT * FROM 'es0528.csv' ORDER BY ts_event LIMIT " +
                                std::to_string(page_size) + " OFFSET " + std::to_string(offset) + ";";
            std::string response = send_query(query);
            std::vector<message> page_messages = parse_csv_response(response);

            if (page_messages.empty()) {
                break;
            }

            all_messages.insert(all_messages.end(), page_messages.begin(), page_messages.end());
            offset += page_size;

            log_info("Fetched " + std::to_string(all_messages.size()) + " messages so far...");
        }

        log_info("Total messages fetched: " + std::to_string(all_messages.size()));
        return all_messages;
    } catch (const std::exception& e) {
        log_error("Error in read_csv_from_questdb: " + std::string(e.what()));
        throw;
    }
}

bool DatabaseManager::test_connection_and_query() {
    try {
        std::string query = "SELECT * FROM 'es0528.csv' LIMIT 10;";
        std::string response = send_query(query);
        log_info("Query response:\n" + response);
        return true;
    } catch (const std::exception& e) {
        log_error("Error in test_connection_and_query: " + std::string(e.what()));
        return false;
    }
}

void DatabaseManager::log_error(const std::string& error_message) {
    std::cerr << "DatabaseManager Error: " << error_message << std::endl;
}

void DatabaseManager::log_info(const std::string& info_message) {
    std::cout << "DatabaseManager Info: " << info_message << std::endl;
}