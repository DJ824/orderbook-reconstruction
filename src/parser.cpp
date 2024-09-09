#include "parser.h"
#include <cstring>
#include <cstdlib>

Parser::Parser(const std::string &file_path) : file_path_(file_path), mapped_file_(nullptr), file_size_(0) {
    message_stream_.reserve(9000000);
}

Parser::~Parser() {
    if (mapped_file_) {
        munmap(mapped_file_, file_size_);
    }
}

void Parser::parse() {
    int fd = open(file_path_.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "error opening file: " << file_path_ << std::endl;
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "error getting file size" << std::endl;
        close(fd);
        return;
    }

    file_size_ = sb.st_size;
    mapped_file_ = static_cast<char*>(mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);

    if (mapped_file_ == MAP_FAILED) {
        std::cerr << "error mapping file" << std::endl;
        return;
    }

    parse_mapped_data();
}

void Parser::parse_mapped_data() {
    char* current = mapped_file_;
    char* end = mapped_file_ + file_size_;

    for (int i = 0; i < 2 && current < end; ++i) {
        current = static_cast<char*>(memchr(current, '\n', end - current));
        if (current) ++current;
        else break;
    }

    while (current < end) {
        char* line_end = static_cast<char*>(memchr(current, '\n', end - current));
        if (!line_end) line_end = end;

        parse_line(current, line_end);

        current = line_end + 1;
    }
}

void Parser::parse_line(const char* start, const char* end) {
    uint64_t ts_event, order_id;
    int32_t price;
    uint32_t size;
    char action, side;

    const char* token_start = start;
    const char* token_end;

    token_end = strchr(token_start, ',');
    ts_event = strtoull(token_start, nullptr, 10);
    //std::cout << "ts_event: " << ts_event << std::endl;
    token_start = token_end + 1;

    action = *token_start;
    //std::cout << "action: " << action << std::endl;
    token_start = strchr(token_start, ',') + 1;

    side = *token_start;
    //std::cout << "side: " << side << std::endl;
    token_start = strchr(token_start, ',') + 1;

    token_end = strchr(token_start, ',');
    price = strtol(token_start, nullptr, 10);
    //std::cout << "price: " << price << std::endl;
    token_start = token_end + 1;

    token_end = strchr(token_start, ',');
    size = strtoul(token_start, nullptr, 10);
    //std::cout << "size: " << size << std::endl;
    token_start = token_end + 1;

    order_id = strtoull(token_start, nullptr, 10);
    //std::cout << "order_id: " << order_id << std::endl;

    bool bid_or_ask = (side == 'B');
    message msg(order_id, ts_event, size, price, action, bid_or_ask);
    message_stream_.push_back(msg);

    //std::cout << "-------------------------" << std::endl;
}


