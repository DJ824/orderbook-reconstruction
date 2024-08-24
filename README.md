# High-Performance Orderbook 

This project implements a high-performance orderbook suitable for algorithmic trading applications. The orderbook efficiently manages buy (bid) and sell (offer) orders, supporting operations like adding, modifying, and removing orders, as well as executing trades. This orderbook was built to process MBO data from databento, specifically for sp500 futures.

## Implementation Details 

One thing I did differently was to include the action of market orders hitting the book. Along with a market order message, there is an addition order filled message that takes into account the effect of the market order on the limit orderbook. 

<img width="769" alt="image" src="https://github.com/user-attachments/assets/8c2e4505-78e2-4374-b3e9-344ca1f85fb6">

Over here we see a market trade message come in at timestamp ending at 11, side is Ask, at price 5300.25 for 2 lots. The next few messages sharing the same timestamp take into account what happens on the book when the market order takes place. We see that one order gets filled ending in order id 16, and the next gets filled at order id 17. We then see cancellation messages for those same id's at the same time, which shows the effect of the market order clearing the size of the limit order and removing from the book. Common implementations just read in the effect of the market order, but I thought it would be a fun challenge to implement market orders themselves. My implementation ignores the fill message, by reading in the trade message, making the action of the market order hit the book, then reading the cancellation message, removing the filled limit order. Next step would be to ignore all cancellation messages that occur after market orders, in order to have the true effect. Could do this by having simply checking size and removing orders. 


## Key Components

1. `order`: Represents individual orders in the orderbook.
2. `limit`: Represents a price level, containing a doubly linked list orders at that price.
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
- Description: Modifies an existing order in the orderbook. If quantity has been increased or if price has changed, reques the order. 

### Trade Order
- Function: `trade_order(uint64_t id, float price, uint32_t size, bool side)`
- Time Complexity: O(1) (accessing the limit object being executed on) + O(m) (number of orders to be filled)
- Description: Executes a market order. 

## Data Structures Used

- `std::map<float, limit*, std::greater<>>`: For storing bid price levels
- `std::map<float, limit*, std::less<>>`: For storing offer price levels
- `std::unordered_map<uint64_t, order*>`: For quick order lookup by ID
- `std::unordered_map<std::pair<float, bool>, limit*, boost::hash<std::pair<float, bool>>>`: For O(1) access to limit objects

## Performance 
- process 8.2 million messages in ~2.7 seconds on M1 Max 32gb
<img width="529" alt="image" src="https://github.com/user-attachments/assets/7d1d7624-f440-4297-bd78-bc0218d460f9">

- Clion has a pretty cool profiling tool, which I used to optimize the performance.
- <img width="923" alt="image" src="https://github.com/user-attachments/assets/f483cfaf-81fe-4103-856b-de2130ac2f45">
- We see here that dynamically allocating new orders in the add limit order function takes up a considerable amount of time, with this implementation the performance was around 5 seconds. I implemented an order pool, preallocating 1000000 orders in a vector, and by integrating it, was able to shave almost 50%.




## Future Plans 
- Gui to visualize the book 
- Backtesting framework to test hft strats
- Add support for multiple instruments like nq, zn, can have seperate message streams for each and a central orderbook class, have one thread on each book maybe
- Matching engine, curently these messages are transmitted by the CME after the customer messages have been processed by their engine. The customer messages also inlcude stop orders, specifically market stop and limit stop. Limit stops only populate the book once the best bid/ask have reached the price of the stop. Then they are added to the doubly linked list of order objects at that price. We would need another 2 unordered_map<float, order*> that include the stop orders, then every iteration of the book we check the best bid/ask and push the orders onto the appropriate price. Handling order matching should be relatively simple, as one cannot submit a limit order above the best bid/ask, it would simply turn into a market order. One could also just have a match function which checks if the best bid crosses the best ask, and if so, iterate through the orders matching them. 


## Visualization 
https://www.youtube.com/watch?v=skqBuDF8bKk
