//
// Created by Devang Jaiswal on 7/29/24.
//

#ifndef DATABENTO_ORDERBOOK_PARSER_H
#define DATABENTO_ORDERBOOK_PARSER_H

#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <iostream>
#include "message.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class Parser {
public:
    explicit Parser(const std::string &file_path);
    ~Parser();
    void parse();
    std::vector<message> message_stream_;

private:
    std::string file_path_;
    char* mapped_file_;
    size_t file_size_;
    void parse_mapped_data();
    void parse_line(const char* start, const char* end);
};

#endif  //DATABENTO_ORDERBOOK_PARSER_H
