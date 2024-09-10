#pragma once
#include <atomic>
#include <thread>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "../include/concurrentqueue.h"
#include "database.cpp"
#include "../include/lock_free_queue.h"


class AsyncLogger {
private:
    struct log_entry {
        std::string timestamp;
        int32_t bid{};
        int32_t ask{};
        int position{};
        int trade_count{};
        float pnl{};
    };

    std::atomic<bool> running_{true};
    //moodycamel::ConcurrentQueue<log_entry> log_queue_;
    LockFreeQueue<log_entry, 1000000> log_queue_;
    std::thread logging_thread_;
    int log_fd_;
    char* log_buffer_;
    size_t buffer_size_;
    size_t buffer_offset_;
    std::mutex console_mutex_;
    bool console_output_;
    DatabaseManager& db_manager_;

    void logging_loop() {
        while (running_ || !log_queue_.empty()) {
            std::optional<log_entry> maybe_entry = log_queue_.dequeue();
            if (maybe_entry) {
                log_entry entry = *maybe_entry;
                std::string log_line = format_log_entry(entry);
                write_to_buffer(log_line);
                if (console_output_) {
                    write_to_console(log_line);
                }
                send_to_database(entry);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        flush_buffer();
    }

    uint64_t format_timestamp(const std::string& timestamp) {
        std::tm tm = {};
        std::istringstream ss(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        tm.tm_hour -= 5;

        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        int milliseconds = 0;
        size_t dot_pos = timestamp.find('.');
        if (dot_pos != std::string::npos) {
            milliseconds = std::stoi(timestamp.substr(dot_pos + 1, 3));
        }

        tp += std::chrono::milliseconds(milliseconds);

        auto duration = tp.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    void send_to_database(const log_entry& entry) {
        uint64_t formatted_timestamp = format_timestamp(entry.timestamp);
        std::stringstream line_protocol;
        line_protocol << "trading_log "
                      << "bid=" << entry.bid << "i,"
                      << "ask=" << entry.ask << "i,"
                      << "position=" << entry.position << "i,"
                      << "trade_count=" << entry.trade_count << "i,"
                      << "pnl=" << std::fixed << std::setprecision(6) << entry.pnl
                      << " " << formatted_timestamp;

        db_manager_.send_line_protocol_tcp(line_protocol.str() + "\n");

    }

    void write_to_buffer(const std::string& log_line) {
        if (buffer_offset_ + log_line.size() > buffer_size_) {
            flush_buffer();
        }
        memcpy(log_buffer_ + buffer_offset_, log_line.c_str(), log_line.size());
        buffer_offset_ += log_line.size();
    }

    void write_to_console(const std::string& log_line) {
        std::lock_guard<std::mutex> lock(console_mutex_);
        std::cout << log_line;
    }

    void flush_buffer() {
        msync(log_buffer_, buffer_size_, MS_SYNC);
        buffer_offset_ = 0;
    }

    static std::string format_log_entry(const log_entry& entry) {
        std::ostringstream oss;
        oss << entry.timestamp << ","
            << std::fixed << std::setprecision(6)
            << entry.bid << ","
            << entry.ask << ","
            << entry.position << ","
            << entry.trade_count << ","
            << entry.pnl << "\n";
        return oss.str();
    }

public:
    AsyncLogger(const std::string& log_file, DatabaseManager& db_manager, bool console_output = false, size_t buffer_size = 10485760)
            : buffer_size_(buffer_size), buffer_offset_(0), console_output_(console_output), db_manager_(db_manager) {
        log_fd_ = open(log_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (log_fd_ == -1) {
            throw std::runtime_error("failed to open log file");
        }
        if (ftruncate(log_fd_, buffer_size_) == -1) {
            throw std::runtime_error("failed to resize log file");
        }
        log_buffer_ = static_cast<char*>(mmap(nullptr, buffer_size_, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd_, 0));
        if (log_buffer_ == MAP_FAILED) {
            throw std::runtime_error("failed to map log file");
        }

        std::string header = "timestamp,bid,ask,position,trade_count,pnl\n";
        write_to_buffer(header);
        if (console_output_) {
            write_to_console(header);
        }

        logging_thread_ = std::thread(&AsyncLogger::logging_loop, this);
    }

    ~AsyncLogger() {
        running_ = false;
        if (logging_thread_.joinable()) {
            logging_thread_.join();
        }
        munmap(log_buffer_, buffer_size_);
        close(log_fd_);
    }

    void log(const std::string& timestamp,
             int32_t bid, int32_t ask, int position,
             int trade_count, float pnl) {
        log_entry entry{timestamp, bid, ask, position,
                        trade_count, pnl};
        log_queue_.enqueue(entry);
    }

    void set_console_output(bool enable) {
        console_output_ = enable;
    }
};