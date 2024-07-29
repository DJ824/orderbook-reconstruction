# High-Performance Orderbook 

This project implements a high-performance orderbook suitable for algorithmic trading applications. The orderbook efficiently manages buy (bid) and sell (offer) orders, supporting operations like adding, modifying, and removing orders, as well as executing trades. This orderbook was built to process MBO data from databento, specifically for sp500 futures (spooz baby!)

## Implementation Details 

One thing I did differently was to include the action of market orders hitting the book. Along with a market order message, there is an addition order filled message that takes into account the effect of the market order on the limit orderbook. 

<img width="769" alt="image" src="https://github.com/user-attachments/assets/8c2e4505-78e2-4374-b3e9-344ca1f85fb6">

Over here we see a market trade message come in at timestamp ending at 11, side is Ask, at price 5300.25 for 2 lots. The next few messages sharing the same timestamp take into account what happens on the book when the market order takes place. We see that one order gets filled ending in order id 16, and the next gets filled at order id 17. We then see cancellation messages for those same id's at the same time, which shows the effect of the market order clearing the size of the limit order and removing from the book. Common implementations just read in the effect of the market order, but I thought it would be a fun challenge to implement market orders themselves. My implementation ignores the fill message, by reading in the trade message, making the action of the market order hit the book, then reading the cancellation message, removing the filled limit order. Next step would be to ignore all cancellation messages that occur after market orders, in order to have the true effect. 


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
- Description: Executes a market order. 

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

## Performance 
- process 8.2 million messages in ~4 seconds 
<img width="826" alt="image" src="https://github.com/user-attachments/assets/492688c4-a31f-4428-9119-c7b9a8b383b8">

## Future Plans 
- gui to visualize the book 
- backtesting framework to test hft strats
- add support for multiple instruments like nq, zn, can have seperate message streams for each and a central orderbook class, have one thread on each book maybe




