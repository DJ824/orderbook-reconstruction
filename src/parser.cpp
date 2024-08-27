//
// Created by Devang Jaiswal on 7/29/24.
//

#include "parser.h"

Parser::Parser(const std::string &file_path) : file_path_(file_path) {
    message_stream_.reserve(9000000);
}

void Parser::parse() {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << file_path_ << std::endl;
        return;
    }

    std::string line;
    if (!std::getline(file, line)) {
        std::cerr << "Error reading the first line or file is empty." << std::endl;
        return;
    }

    while (std::getline(file, line)) {
        parse_line(line);
    }

    file.close();
}

void Parser::parse_line(const std::string &line) {
    std::istringstream ss(line);
    std::string token;

    std::getline(ss, token, ',');
    uint64_t ts_event, order_id;
    float price;
    uint32_t size;
    std::string action, side;

    std::getline(ss, token, ',');
    ts_event = std::stoull(token);

    for (int i = 0; i < 3; ++i)
        std::getline(ss, token, ',');

    std::getline(ss, action, ',');
    std::getline(ss, side, ',');
    std::getline(ss, token, ',');
    price = std::stof(token);
    std::getline(ss, token, ',');
    size = std::stoul(token);

    for (int i = 0; i < 1; ++i)
        std::getline(ss, token, ',');
    std::getline(ss, token, ',');
    order_id = std::stoull(token);

    bool bid_or_ask = (side == "B");
    message msg(order_id, ts_event, size, price, action[0], bid_or_ask);
    message_stream_.push_back(msg);
}

