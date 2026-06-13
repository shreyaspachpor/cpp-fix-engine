# FIX Trading Exchange & Matching Engine

A high-performance C++17 trading acceptor and price-time priority matching engine, combined with an interactive real-time Terminal UI (TUI) client for order entry and book visualization.

The system communicates over the industry-standard **FIX 4.2 protocol** to handle the full order lifecycle (New, Fill, Partial Fill, Cancel, Replace).

---

## Architecture & System Design

```
   +-----------------------------------------------------------+
   |                      python TUI Client                    |
   |   - Interactive Command Console (buy/sell/cancel/switch)  |
   |   - Real-Time Order Book Depth & Active User Orders       |
   +-----------------------------------------------------------+
                                |
                   FIX 4.2 TCP connection (Port 5001)
                                |
   +-----------------------------------------------------------+
   |                     C++ FIX Acceptor                      |
   |   - Session Manager (QuickFIX/C++: Logons & Heartbeats)   |
   |   - Bidirectional ClOrdID <-> OrderID State Mapping       |
   +-----------------------------------------------------------+
                                |
   +-----------------------------------------------------------+
   |                      Matching Engine                      |
   |   - Price-Time Priority Order Books (RELIANCE, INFY, TCS) |
   |   - O(log N) price indexing, O(1) queue processing        |
   +-----------------------------------------------------------+
```

### Key Technical Details

* **Matching Logic**: Uses a Price-Time priority matching engine. Resting orders are indexed in sorted STL maps (`std::map<Price, std::deque<Order>>`) representing price levels, with double-ended queues handling FIFO execution priority.
* **Bilateral Execution Reports**: When matches execute, the C++ acceptor maps internal Order IDs back to client ClOrdIDs and dispatches fill confirmations (`ExecutionReport` tags `31` and `32`) to both the buyer and seller sessions.
* **Real-Time Order Book Synchronization**: The C++ acceptor maintains a list of all active sessions and broadcasts all order status transitions (New, Cancel, Replace, Fill) to all clients in real time. The Python clients use these broadcasts to dynamically display a unified public order book depth while keeping their local private order panel separated.
* **Asynchronous Client Dashboard**: The Python TUI uses standard sockets (no third-party dependencies) and utilizes the Windows console `msvcrt` module to poll for user inputs character-by-character. If background execution updates or heartbeats arrive from the server, the client immediately updates the book view without interrupting your active typing.
* **Heartbeat Verification**: Real-time interceptors print inbound/outbound heartbeat events in the server console to verify connection lifecycles.

---

## Project Structure

```
trading_system/
├── apps/
│   ├── exchange_fix_main.cpp  # C++ FIX Server Entrypoint
│   └── fix_tui_client.py      # Python TUI Dashboard Client
├── config/
│   └── fix_session.cfg        # QuickFIX Acceptor Session Profile
├── include/
│   ├── core/                  # OMS, Order, Trade interfaces
│   ├── fix/                   # C++ FIX Acceptor Application class
│   └── matching/              # OrderBook matching structures
├── src/
│   ├── fix/                   # Inbound Message Crackers & Outbound Reports
│   └── matching/              # Order Book Priority execution
└── run_demo.bat               # Windows One-Click Demo Script
```

---

## Building the Project

The workspace is configured for **MSVC Release** builds (using MSVC is required to link against the compiled `quickfix.lib` static library on Windows).

Run the following commands in your terminal to build:

```powershell
# Generate MSVC build files
cmake -B build_msvc -G "Visual Studio 17 2022"

# Build executable in Release configuration
cmake --build build_msvc --config Release
```

The compiled binary `exchange_fix.exe` will be located in `build_msvc/Release/`.

---

## Running the System

You can run the exchange in either **Single-Session** (the default demo loop) or **Multi-Session Concurrent Mode** (allowing multiple independent client terminals to trade concurrently).

---

### Option 1: Single-Session Demo (One-Click)

For a quick demonstration of the TUI console on Windows, run the batch script at the project root:

```powershell
.\run_demo.bat
```

This will:
1. Start the **C++ FIX Acceptor Server** in a minimized console window (`FIX EXCHANGE SERVER`).
2. Pre-populate mock order book depth for three active tickers (`RELIANCE`, `INFY`, `TCS`).
3. Launch the **Interactive TUI Console** as client `CLIENT` in your active terminal.

---

### Option 2: Multi-Session Concurrent Mode (Concurrently Trade Multiple Symbols)

The exchange server supports dynamic per-symbol locking and multi-session capabilities on port `5001`. You can launch multiple distinct client windows to trade separate symbols concurrently or trade against each other:

1. **Start the C++ Acceptor Server**:
   ```powershell
   .\build_msvc\Release\exchange_fix.exe
   ```

2. **Launch Client Session 1 (`CLIENT`)**:
   Open a separate command prompt window and run:
   ```powershell
   python apps/fix_tui_client.py CLIENT
   ```
   *Note: Running without arguments also defaults to `CLIENT`.*

3. **Launch Client Session 2 (`CLIENT1`)**:
   Open another command prompt window and run:
   ```powershell
   python apps/fix_tui_client.py CLIENT1
   ```

4. **Launch Client Session 3 (`CLIENT2`)**:
   Open another command prompt window and run:
   ```powershell
   python apps/fix_tui_client.py CLIENT2
   ```

#### Concurrency & Routing Mechanics:
* **Dynamic Per-Ticker Locks**: Transactions on `RELIANCE` (Session 1) and `INFY` (Session 2) run in parallel on different CPU threads without interfering with each other or causing mutex wait contention.
* **Bilateral Match Updates**: If client `CLIENT` submits a `buy 100 120.00` on `RELIANCE` and client `CLIENT1` submits a `sell 100 120.00` on `RELIANCE`, the matching engine will pair them up and route the buy fill report back to `CLIENT`'s console, and the sell fill report back to `CLIENT1`'s console.

---

### Client Console Commands

Use the following commands inside any active TUI console:

* **`buy <qty> <price>`** / **`sell <qty> <price>`**: Submits a limit order for the active symbol.
* **`cancel <order_id>`**: Cancels a resting order (e.g. `cancel USR-0001`).
* **`switch <symbol>`**: Switches the visible dashboard view (e.g. `switch INFY` to view INFY's order book).
* **`r`**: Force-refreshes the layout.
* **`q`**: Closes the client connection and exits.
