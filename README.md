# Trading System

A high-performance order matching engine and Order Management System mainly built in C++17.

## Features (Implemented)

- **Order Book**: Standard price-time priority matching.
- **OMS**: Basic Order Management System handling new orders, executions, and cancellations.
- **Order Lifecycle**: Support for New, Filled, Partially Filled, and Cancelled states.
- **Support for Indian Stocks**: Validated with RELIANCE, TCS, INFY.

## Project Structure

```
trading_system/
├── include/
│   ├── common/          # Types (OrderId, Price) and Enums
│   ├── core/            # Order, OrderState, OMS
│   └── matching/        # OrderBook logic
├── src/
│   ├── matching/
│   └── oms/             # OMS implementation
├── apps/
│   └── exchange_main.cpp   # Main driver with Indian stock examples
└── CMakeLists.txt
```

## Building

### Using MSYS2/MinGW (Windows)

```bash
cd trading_system
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Running

```bash
.\build\exchange.exe
```

## Roadmap

- [ ] **Phase 3**: Modify Order support (Cancel/Replace)
- [ ] **Phase 4**: FIX Protocol (Financial Information eXchange)
- [ ] **Phase 5**: Performance optimization

## Requirements

- C++17 or later
- CMake 3.16+
