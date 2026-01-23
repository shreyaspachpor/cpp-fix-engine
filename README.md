# Trading System

A high-performance order matching engine built in C++17.

## Features (Phase 1)

- **Order Book Implementation**: Efficient price-time priority matching
- **Market Orders**: Support for buy and sell limit orders
- **Order Matching**: Automatic execution of matching orders at best prices
- **Price Levels**: Sorted bid/ask queues using STL containers

## Project Structure

```
trading_system/
├── include/
│   ├── common/          # Common types and enums
│   │   ├── enums.h      # Side, OrderType, ExecType
│   │   └── types.h      # OrderId, Price, Quantity, etc.
│   ├── core/            # Core business objects
│   │   ├── execution.h  # Execution/Trade representation
│   │   └── order.h      # Order structure
│   └── matching/        # Order matching logic
│       └── order_book.h # OrderBook class
├── src/
│   └── matching/
│       └── order_book.cpp  # Order book implementation
├── apps/
│   └── exchange_main.cpp   # Test driver
└── CMakeLists.txt
```

## Building

### Using MSYS2/MinGW (Windows)

```bash
cd trading_system
g++ -std=c++17 -Iinclude -o build/exchange.exe apps/exchange_main.cpp src/matching/order_book.cpp
```

### Using CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Running

```bash
.\build\exchange.exe
```

## Order Book Logic

- **Bids**: Sorted in descending order (highest price first) using `std::greater<Price>`
- **Asks**: Sorted in ascending order (lowest price first, default)
- **Matching**:
  - Buy orders match against the best (lowest) ask price
  - Sell orders match against the best (highest) bid price
  - Executions occur at the resting order's price
  - Partial fills are supported

## Example

```cpp
OrderBook book;

Order buyOrder{1, 100.0, "TCS", Side::Buy, OrderType::Limit, 10, 10};
Order sellOrder{2, 100.0, "TCS", Side::Sell, OrderType::Limit, 5, 5};

auto executions = book.process_order(buyOrder);  // Added to bid side
auto executions2 = book.process_order(sellOrder); // Matches 5 shares @ 100.0
```

## Roadmap

- [ ] Phase 2: Order management system (OMS)
- [ ] Phase 3: Risk management
- [ ] Phase 4: FIX protocol support
- [ ] Phase 5: Performance optimization

## Requirements

- C++17 or later
- CMake 3.16+ (optional)
- MSYS2/MinGW or any C++ compiler
