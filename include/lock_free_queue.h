#ifndef DATABENTO_ORDERBOOK_LOCK_FREE_QUEUE_H
#define DATABENTO_ORDERBOOK_LOCK_FREE_QUEUE_H
#include <atomic>
#include <array>
#include <optional>

// single producer single consumer lock free queue, one thread for producing log entries, and one thread for consuming
// use fixed size circular buffer to store the elements, each element in buffer is a Node containing data and bool flag to indicate whether data has been written

template <typename T, size_t SIZE>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<bool> written{false};
        T data;
    };

    std::array<Node, SIZE> buffer_;

    // atomics for thread safety
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};

public:
    LockFreeQueue() = default;

    bool enqueue(const T& item) {
       auto curr_tail = tail_.load();
       auto next_tail = (curr_tail + 1) % SIZE;

       // if the next node after tail is head, queue is full

       if (next_tail == head_.load()) {
           return false;
       }

       // store true in the current tail node to indicate data has been written
       buffer_[curr_tail].data = item;
       buffer_[curr_tail].written.store(true);
       // update tail
       tail_.store(next_tail);

       return true;
    }

    std::optional<T> dequeue() {
        auto curr_head = head_.load();

        // if head == tail, queue is empty
        if (curr_head == tail_.load()) {
            return std::nullopt;
        }

        // if written is false, writer thread has not finished writing data
        if (!buffer_[curr_head].written.load()) {
            return std::nullopt;
        }

        // get the data from head node, write false to indicate no data has been written, and update head
        T result = buffer_[curr_head].data;
        buffer_[curr_head].written.store(false);
        head_.store((curr_head + 1) % SIZE);

        return result;
    }

    bool empty() const {
        return (head_.load() == tail_.load());
    }

};

/*
 std::memory_order is an enum in c++ that specifies how memory access around atomic operations should be ordered relative to similar operations in other threads
 - each thread has its own view of memory
 - compilers and CPUs can reorder memory operations for optimization
 - due to caching, mem operations can become visible to different threads at different ties

 types of memory order:
 1) memory_order_relaxed
 - provides only atomicity with no synchonizatoin or ordering guarantees
 - allows for maximum reordering of operations
 - use case: incrementing a global counter where the exact order does not matter
 - least restrictive and fastest

 2) memory_order_acquire
 - used for load operations (reading)
 - ensures that subsequent read and write operations in same thread cannt be reorderd before this load
 - synchronized with a release operation on the same variable in another thread
 - creates a happens before realtionship with the release operation it synchronizes with

 3) memory_order_relaese
 - used for store operations (writing)
 - ensures that prior read and write operations in the same thread cannot be reoredered after this store
 - syynchonized with an acquire operation on the same variable in another thread
 - creates a happens before relationship with the acquire operation it synchronizes with

 4) memory_order_acq_rel
 - combines aquire and realease semantics
 - used for read-modify-write operations
 - prevents reordering of memory operations both before and after the atomic operation
 - synchronzies with both acquire and release operations in other threads

 */


#endif //DATABENTO_ORDERBOOK_LOCK_FREE_QUEUE_H
