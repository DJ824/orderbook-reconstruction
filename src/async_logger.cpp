#include <atomic>
#include <thread>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include "../include/concurrentqueue.h"

class AsyncLogger {
private:
    struct log_entry {
        std::string timestamp;
        float bid;
        float ask;
        int position;
        int trade_count;
        float pnl;
    };

    std::atomic<bool> running_{true};
    moodycamel::ConcurrentQueue<log_entry> file_queue_;
    moodycamel::ConcurrentQueue<log_entry> console_queue_;

    std::vector<std::thread> threads_;
    int log_fd_;
    char* log_buffer_;
    size_t buffer_size_;
    size_t buffer_offset_;
    std::mutex console_mutex_;
    bool console_output_;

    void file_loop() {
        while (running_ || file_queue_.size_approx() > 0) {
            log_entry entry;
            if (file_queue_.try_dequeue(entry)) {
                std::string log_line = format_log_entry(entry);
                write_to_buffer(log_line);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        flush_buffer();
    }

    void console_loop() {
        while (running_ || console_queue_.size_approx() > 0) {
            log_entry entry;
            if (console_queue_.try_dequeue(entry)) {
                std::string log_line = format_log_entry(entry);
                write_to_console(log_line);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
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
    AsyncLogger(const std::string& log_file, bool console_output = false, size_t buffer_size = 10485760)
            : buffer_size_(buffer_size), buffer_offset_(0), console_output_(console_output) {
        log_fd_ = open(log_file.c_str(), O_RDWR | O_CREAT , S_IRUSR | S_IWUSR);
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

        threads_.emplace_back(&AsyncLogger::file_loop, this);
        if (console_output_) {
            threads_.emplace_back(&AsyncLogger::console_loop, this);
        }
    }

    ~AsyncLogger() {
        running_ = false;
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        munmap(log_buffer_, buffer_size_);
        close(log_fd_);
    }

    void log(const std::string& timestamp,
             float bid, float ask, int position,
             int trade_count, float pnl) {
        log_entry entry{timestamp, bid, ask, position,
                        trade_count, pnl};
        file_queue_.enqueue(entry);
        if (console_output_) {
            console_queue_.enqueue(entry);
        }
    }

    void set_console_output(bool enable) {
        console_output_ = enable;
    }
};