# High-Performance Orderbook 

This project implements a high-performance orderbook and backtesting framework suitable for algorithmic trading applications. The orderbook efficiently manages buy (bid) and sell (offer) orders, supporting operations like adding, modifying, and removing orders, as well as executing trades. This orderbook was built to process MBO data from databento, specifically for sp500 futures.

## Implementation Details 

One thing I did differently was to include the action of market orders hitting the book. Along with a market order message, there is an addition order filled message that takes into account the effect of the market order on the limit orderbook. 

<img width="769" alt="image" src="https://github.com/user-attachments/assets/8c2e4505-78e2-4374-b3e9-344ca1f85fb6">

Over here we see a market trade message come in at timestamp ending at 11, side is Ask, at price 5300.25 for 2 lots. The next few messages sharing the same timestamp take into account what happens on the book when the market order takes place. We see that one order gets filled ending in order id 16, and the next gets filled at order id 17. We then see cancellation messages for those same id's at the same time, which shows the effect of the market order clearing the size of the limit order and removing from the book. Common implementations just read in the effect of the market order, but I thought it would be a fun challenge to implement market orders themselves. My implementation ignores the fill message, by reading in the trade message, making the action of the market order hit the book, then reading the cancellation message, removing the filled limit order. We accomplish 
this by accessing the limit object that the price of the trade order represents, then iterating through the list of orders until all size of the market order has been depleted.

Update 08/30/24
Added database integration (QuestDB using Influx Line Protocol) for book snapshots/message, gui to visualize the book using d3.js and web workers to improve performance, and a backtesting framework incorporating a 2 hft strategies based on book imbalance/skew and vwap.
In addition, we use an asynchronous logger which uses 2 lock free queues to relay the results of the backtester, writing to std out and a csv file with a seperate thread for each. 

## Key Components

1. `order`: Represents individual orders in the orderbook.
2. `limit`: Represents a price level, containing a doubly linked list orders at that price.
3. `orderbook`: The main class that manages the entire orderbook structure. 

## Orderbook Operations and Time Complexities

### Add Limit Order
- Function: `add_limit_order(uint64_t id, int32_t price, uint32_t size, bool side, uint64_t unix_time)`
- Time Complexity: O(1) average case, O(log n) worst case when creating a new limit
- Description: Adds a new limit order to the orderbook.

### Remove Order
- Function: `remove_order(uint64_t id, int32_t price, uint32_t size, bool side)`
- Time Complexity: O(1) average case, O(log n) worst case when removing a limit
- Description: Removes an existing order from the orderbook.

### Modify Order
- Function: `modify_order(uint64_t id, int32_t new_price, uint32_t new_size, bool side, uint64_t unix_time)`
- Time Complexity: O(1) average case, O(log n) worst case when creating or removing a limit
- Description: Modifies an existing order in the orderbook. If quantity has been increased or if price has changed, reques the order. 

### Trade Order
- Function: `trade_order(uint64_t id, int32_t price, uint32_t size, bool side)`
- Time Complexity: O(1) (accessing the limit object being executed on) + O(m) (number of orders to be filled)
- Description: Executes a market order. Get the limit object that the market order executed on and iterate through the list of orders, filling them. 

### Trade Order
- Function: `get_or_insert_limit(bool side, int32_t price);`
- Time Complexity: O(1) average, worst case O(log(n)) when creating new limit objects to insert into maps 
- Description: Gets a limit object for the requested price and side. We have an unordered_map that we use to store all current limit objects for fast access, if we have to create a new limit object, we store in the appropraite side. 

## Data Structures Used

- `std::map<float, limit*, std::greater<>>`: For storing bid price levels
- `std::map<float, limit*, std::less<>>`: For storing offer price levels
- `std::unordered_map<uint64_t, order*>`: For quick order lookup by ID
- `std::unordered_map<std::pair<int32_t, bool>, limit*, boost::hash<std::pair<int32_t, bool>>>`: For O(1) access to limit objects

## Performance 
- process 8.2 million messages in ~2.7 seconds on M1 Max 32gb
<img width="529" alt="image" src="https://github.com/user-attachments/assets/7d1d7624-f440-4297-bd78-bc0218d460f9">

- Clion has a pretty cool profiling tool, which I used to optimize the performance.
 <img width="923" alt="image" src="https://github.com/user-attachments/assets/f483cfaf-81fe-4103-856b-de2130ac2f45">
- We see here that dynamically allocating new orders in the add limit order function takes up a considerable amount of time, with this implementation the performance was around 5 seconds. I implemented an order pool, preallocating 1000000 orders in a vector, and by integrating it, was able to shave almost 50%.
Update 09/5/24: Memory mapped the csv file and optimized the way we calculate the total bid and ask vol of the first 100 levels, total time to run the backtester including csv parsing is around ~8.2 seconds for a day's worth of market data (nyc open to close) 
https://gifyu.com/image/S1vuT


## Future Plans 
- Gui to visualize the book - done
- Backtesting framework to test hft strats - done
- Add support for multiple instruments like nq, zn, to run strats across them - todo 
- Visualize the results of the strategies - todo
- Optimization via low latency techniques - todo 

## Visualization 
<img width="899" alt="image" src="https://github.com/user-attachments/assets/b95389a0-15f7-4773-8cff-2eeba34af2de">

https://www.youtube.com/watch?v=0h6425tUcWI

