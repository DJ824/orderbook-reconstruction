//
// Created by Devang Jaiswal on 7/29/24.
//

#ifndef DATABENTO_ORDERBOOK_PARSER_H
#define DATABENTO_ORDERBOOK_PARSER_H
#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include "message.h"

class parser {
public:
    explicit parser(const std::string &file_path);
    void parse();
    std::vector<message> message_stream;

private:
    std::string file_path_;
    void parse_line(const std::string &line);

};


#endif //DATABENTO_ORDERBOOK_PARSER_H
