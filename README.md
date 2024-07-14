# High-Performance Orderbook Implementation

This project implements a high-performance orderbook suitable for algorithmic trading applications. The orderbook efficiently manages buy (bid) and sell (offer) orders, supporting operations like adding, modifying, and removing orders, as well as executing trades. This orderbook was built to process MBO data from databento. 

## Key Components

1. `order`: Represents individual orders in the orderbook.
2. `limit`: Represents a price level, containing orders at that price.
3. `orderbook`: The main class that manages the entire orderbook structure.

## Orderbook Operations and Time Complexities

### Add Limit Order
- Function: `add_limit_order(uint64_t id, float price, uint32_t size, bool side, uint64_t unix_time)`
- Time Complexity: O(1) average case, O(log n) worst case when creating a new limit
- Description: Adds a new limit order to the orderbook.

### Remove Order
- Function: `remove_order(uint64_t id, float price, uint32_t size, bool side)`
- Time Complexity: O(1) average case, O(log n) worst case when removing a limit
- Description: Removes an existing order from the orderbook.

### Modify Order
- Function: `modify_order(uint64_t id, float new_price, uint32_t new_size, bool side, uint64_t unix_time)`
- Time Complexity: O(1) average case, O(log n) worst case when creating or removing a limit
- Description: Modifies an existing order in the orderbook.

### Trade Order
- Function: `trade_order(uint64_t id, float price, uint32_t size, bool side)`
- Time Complexity: O(m), where m is the number of orders that need to be filled
- Description: Executes a trade, matching orders at the specified price level.

### Get Best Bid/Ask
- Functions: `get_best_bid()`, `get_best_ask()`
- Time Complexity: O(1)
- Description: Returns the best (highest) bid or (lowest) ask price level.

### Get Best Bid/Ask Price
- Functions: `get_best_bid_price()`, `get_best_ask_price()`
- Time Complexity: O(1)
- Description: Returns the price of the best bid or ask.

### Get Order Count
- Function: `get_count()`
- Time Complexity: O(1)
- Description: Returns the total number of orders in the orderbook.

## Data Structures Used

- `std::map<float, limit*, std::greater<>>`: For storing bid price levels
- `std::map<float, limit*, std::less<>>`: For storing offer price levels
- `std::unordered_map<uint64_t, order*>`: For quick order lookup by ID
- `std::unordered_map<std::pair<float, bool>, limit*, boost::hash<std::pair<float, bool>>>`: For O(1) access to limit objects

## Performance Improvements

- The use of `limit_lookup_` provides O(1) average case access to limit objects, significantly improving the performance of add, modify, and remove operations.
- `std::map` is still used for bids and offers to maintain price-time priority and allow efficient iteration through price levels.
- The orderbook maintains iterators to the best bid and offer for O(1) access to the top of the book.

## Future Improvements

- Implement thread-safety for concurrent access
- Optimize memory management with custom allocators or object pools
- Add support for different order types (e.g., IOC, FOK)
- Implement a market data feed generation system

## Preview
https://www.youtube.com/watch?v=FcP9N1QjSW4



