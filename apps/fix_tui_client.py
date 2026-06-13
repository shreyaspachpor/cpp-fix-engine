import socket
import datetime
import time
import threading
import os
import sys

# Try to import msvcrt for Windows non-blocking input
try:
    import msvcrt
    HAS_MSVCRT = True
except ImportError:
    HAS_MSVCRT = False

# Color codes
COLOR_GREEN = "\033[1;32m"
COLOR_RED = "\033[1;31m"
COLOR_YELLOW = "\033[1;33m"
COLOR_CYAN = "\033[1;36m"
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_GRAY = "\033[90m"

class FixTuiClient:
    def __init__(self, host="127.0.0.1", port=5001, client_id="CLIENT"):
        self.host = host
        self.port = port
        self.client_id = client_id
        self.sock = None
        self.seq_num = 1
        self.running = False
        self.buffer = b""
        self.recv_thread = None
        
        # State tracking
        self.active_symbol = "RELIANCE"
        self.orders = {}       # cl_ord_id -> order details
        self.trades = []       # list of formatted trade logs
        self.bg_order_seq = 1
        self.user_order_seq = 1
        self.log_messages = []  # general system logs
        self.connected_event = threading.Event()
        self.dirty = False     # Needs TUI redraw

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.running = True
            self.recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self.recv_thread.start()
            self.log("Connected to FIX server.")
            self.connected_event.set()
        except Exception as e:
            self.log(f"Connection error: {e}")
            raise e

    def disconnect(self):
        self.running = False
        if self.sock:
            self.sock.close()
        self.log("Disconnected.")

    def log(self, msg):
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        # Highlight errors or failures in red
        if any(w in msg.lower() for w in ("error", "reject", "failed")):
            formatted = f"{COLOR_RED}{msg}{COLOR_RESET}"
        else:
            formatted = msg
        self.log_messages.append(f"[{timestamp}] {formatted}")
        if len(self.log_messages) > 10:
            self.log_messages.pop(0)
        self.dirty = True

    def _recv_loop(self):
        while self.running:
            try:
                data = self.sock.recv(4096)
                if not data:
                    break
                self.buffer += data
                self._parse_buffer()
            except Exception:
                break
        self.running = False

    def _parse_buffer(self):
        while b"8=FIX.4.2\x01" in self.buffer:
            start = self.buffer.index(b"8=FIX.4.2\x01")
            len_start = start + len("8=FIX.4.2\x01")
            if b"\x01" not in self.buffer[len_start:]:
                break
            len_end = self.buffer.index(b"\x01", len_start)
            body_len_str = self.buffer[len_start:len_end].split(b"=")[1]
            body_len = int(body_len_str)

            total_len = len_end + 1 + body_len + 7
            if len(self.buffer) < total_len:
                break

            msg_bytes = self.buffer[start:total_len]
            self.buffer = self.buffer[total_len:]
            self._handle_message(msg_bytes)

    def _handle_message(self, msg_bytes):
        msg_str = msg_bytes.decode('ascii', errors='replace')
        fields = {}
        for item in msg_str.split("\x01"):
            if "=" in item:
                k, v = item.split("=", 1)
                fields[k] = v

        msg_type = fields.get("35")
        if msg_type == "A":
            self.log("FIX session logged on.")
        elif msg_type == "8": # Execution Report
            cl_ord_id = fields.get("11", "")
            exec_type = fields.get("150", "")
            ord_status = fields.get("39", "")
            leaves_qty = int(fields.get("151", "0"))
            cum_qty = int(fields.get("14", "0"))
            price = float(fields.get("44", "0.00"))
            symbol = fields.get("55", "")
            side = fields.get("54", "") # '1' = Buy, '2' = Sell
            
            # Map side string
            side_str = "BUY" if side == '1' else "SELL"

            # Check if this order already exists in our tracking
            if cl_ord_id not in self.orders:
                # Store new order state
                self.orders[cl_ord_id] = {
                    "cl_ord_id": cl_ord_id,
                    "symbol": symbol,
                    "side": side_str,
                    "price": price,
                    "original_qty": leaves_qty + cum_qty,
                    "leaves_qty": leaves_qty,
                    "status": "New"
                }
            else:
                self.orders[cl_ord_id]["leaves_qty"] = leaves_qty
            
            status_map = {'0':'New','1':'PartialFill','2':'Filled','4':'Cancelled','8':'Rejected'}
            self.orders[cl_ord_id]["status"] = status_map.get(ord_status, ord_status)

            # Record trade prints if execution reports show fills
            last_shares = int(fields.get("32", "0")) # Tag 32 (LastShares)
            last_px = float(fields.get("31", "0.00"))  # Tag 31 (LastPx)
            
            if last_shares > 0 and exec_type in ('1', '2'): # Partial Fill or Fill
                # De-duplicate double match logs by only adding to Times & Sales for BUY side
                if side == '1':
                    trade_time = datetime.datetime.now().strftime("%H:%M:%S")
                    trade_desc = f"[{trade_time}] TRADE: {last_shares} {symbol} @ {last_px:.2f}"
                    self.trades.append(trade_desc)
                self.log(f"Execution fill: {side_str} {last_shares} {symbol} @ {last_px:.2f}")

        elif msg_type == "9": # Order Cancel Reject
            cl_ord_id = fields.get("11", "")
            reason = fields.get("58", "")
            self.log(f"Cancel Rejected for {cl_ord_id}: {reason}")
        self.dirty = True

    def send_message(self, msg_type, fields):
        sending_time = datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S.%f")[:-3]
        body_fields = [
            (35, msg_type),
            (49, self.client_id),
            (56, "EXCHANGE"),
            (34, str(self.seq_num)),
            (52, sending_time)
        ]
        self.seq_num += 1
        for k, v in fields:
            body_fields.append((k, str(v)))

        body_str = "".join(f"{k}={v}\x01" for k, v in body_fields)
        body_bytes = body_str.encode('ascii')
        body_len = len(body_bytes)

        header_str = f"8=FIX.4.2\x019={body_len}\x01"
        header_bytes = header_str.encode('ascii')

        msg_without_checksum = header_bytes + body_bytes
        checksum = sum(msg_without_checksum) % 256
        checksum_str = f"10={checksum:03d}\x01"

        full_msg = msg_without_checksum + checksum_str.encode('ascii')
        self.sock.sendall(full_msg)

    def submit_bg_order(self, symbol, side, price, qty):
        cl_ord_id = f"BG-{symbol[:3]}-{self.bg_order_seq:04d}"
        self.bg_order_seq += 1
        self.send_message("D", [
            (11, cl_ord_id),
            (21, 1),
            (55, symbol),
            (54, 1 if side == "BUY" else 2),
            (40, 2),
            (44, f"{price:.2f}"),
            (38, qty),
            (59, 0),
            (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
        ])
        # Add to local tracker immediately (status will update on exec report)
        self.orders[cl_ord_id] = {
            "cl_ord_id": cl_ord_id,
            "symbol": symbol,
            "side": side,
            "price": price,
            "original_qty": qty,
            "leaves_qty": qty,
            "status": "Pending"
        }

    def submit_user_order(self, side, symbol, qty, price):
        cl_ord_id = f"{self.client_id}-USR-{self.user_order_seq:04d}"
        self.user_order_seq += 1
        self.send_message("D", [
            (11, cl_ord_id),
            (21, 1),
            (55, symbol),
            (54, 1 if side == "BUY" else 2),
            (40, 2),
            (44, f"{price:.2f}"),
            (38, qty),
            (59, 0),
            (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
        ])
        self.log(f"Submitted {side} order {cl_ord_id}: {qty} {symbol} @ {price:.2f}")

    def cancel_order(self, cl_ord_id):
        if cl_ord_id not in self.orders:
            self.log(f"Error: Order ID {cl_ord_id} not found locally.")
            return
        
        ord_info = self.orders[cl_ord_id]
        if ord_info["status"] in ("Filled", "Cancelled", "Rejected"):
            self.log(f"Error: Order {cl_ord_id} is already in terminal state ({ord_info['status']}).")
            return

        cancel_id = f"CNL-{cl_ord_id}"
        self.send_message("F", [
            (11, cancel_id),
            (41, cl_ord_id),
            (55, ord_info["symbol"]),
            (54, 1 if ord_info["side"] == "BUY" else 2),
            (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
        ])
        self.log(f"Requested cancellation of {cl_ord_id}")

    def pre_populate_books(self):
        self.log("Pre-populating order books...")
        # RELIANCE
        self.submit_bg_order("RELIANCE", "BUY", 99.50, 100)
        self.submit_bg_order("RELIANCE", "BUY", 99.00, 150)
        self.submit_bg_order("RELIANCE", "SELL", 100.50, 80)
        self.submit_bg_order("RELIANCE", "SELL", 101.00, 120)
        
        # INFY
        self.submit_bg_order("INFY", "BUY", 1495.00, 40)
        self.submit_bg_order("INFY", "BUY", 1490.00, 60)
        self.submit_bg_order("INFY", "SELL", 1505.00, 30)
        self.submit_bg_order("INFY", "SELL", 1510.00, 50)
        
        # TCS
        self.submit_bg_order("TCS", "BUY", 3200.00, 20)
        self.submit_bg_order("TCS", "SELL", 3220.00, 30)
        self.log("Pre-population done.")

    def render_tui(self, input_buffer=""):
        if os.name == 'nt':
            os.system(f"title CM TRADING SYSTEM - ORDER CONSOLE ({self.client_id})")
            
        os.system('cls' if os.name == 'nt' else 'clear')
        
        # Gather order book depth for active symbol
        bids = {} # price -> qty
        asks = {} # price -> qty

        for o in self.orders.values():
            if o["symbol"] == self.active_symbol and o["leaves_qty"] > 0 and o["status"] not in ("Filled", "Cancelled", "Rejected"):
                if o["side"] == "BUY":
                    bids[o["price"]] = bids.get(o["price"], 0) + o["leaves_qty"]
                else:
                    asks[o["price"]] = asks.get(o["price"], 0) + o["leaves_qty"]

        sorted_bids = sorted(bids.items(), key=lambda x: x[0], reverse=True)
        sorted_asks = sorted(asks.items(), key=lambda x: x[0])

        # Gather maximum quantity to scale depth bars dynamically
        max_qty = 1
        for q in bids.values():
            if q > max_qty:
                max_qty = q
        for q in asks.values():
            if q > max_qty:
                max_qty = q

        # Compute spread
        if sorted_asks and sorted_bids:
            spread = sorted_asks[0][0] - sorted_bids[0][0]
            spread_line = f" SPREAD: {spread:.2f} ".center(45, "-")
            spread_color = COLOR_YELLOW
        else:
            spread_line = " SPREAD: -.-- ".center(45, "-")
            spread_color = COLOR_GRAY

        # Get user orders (USR-xxx) to show on the right table
        user_orders = [o for o in self.orders.values() if o["cl_ord_id"].startswith(f"{self.client_id}-USR-") and o["leaves_qty"] > 0]
        user_orders = sorted(user_orders, key=lambda x: x["cl_ord_id"])[-8:] # last 8 orders

        # 1. Header Box (Width 88)
        symbol_list_str = ""
        for s in ["RELIANCE", "INFY", "TCS"]:
            if s == self.active_symbol:
                symbol_list_str += f"{COLOR_BOLD}{COLOR_GREEN}[{s}]{COLOR_RESET}  "
            else:
                symbol_list_str += f"{COLOR_GRAY}{s}{COLOR_RESET}  "
                
        left_str = f" Symbols: {symbol_list_str}"
        raw_left = left_str.replace(COLOR_GREEN, "").replace(COLOR_RESET, "").replace(COLOR_BOLD, "").replace(COLOR_GRAY, "")
        left_padding = " " * (40 - len(raw_left))
        
        right_str = " Acceptor: 127.0.0.1:5001 (FIX 4.2)"
        right_padding = " " * (41 - len(right_str))

        print(f"+{ '-' * 86 }+")
        print("|" + " " * 22 + f"{COLOR_BOLD}CM TRADING SYSTEM - INTERACTIVE FIX CLIENT{COLOR_RESET}" + " " * 22 + "|")
        print(f"+{ '-' * 42 }+{ '-' * 43 }+")
        print(f"|{left_str}{left_padding}|{right_str}{right_padding}|")
        print(f"+{ '-' * 42 }+{ '-' * 43 }+")

        # 2. Main Body Box (Left 45, Right 41)
        title_left = f"ORDER BOOK DEPTH ({self.active_symbol})".center(45)
        title_right = "MY ACTIVE USER ORDERS".center(41)
        print(f"+{ '-' * 45 }+{ '-' * 41 }+")
        print(f"|{title_left}|{title_right}|")
        print(f"|  Price     Volume    Depth Bar              |  ID       Side   Price    Qty  Status     |")
        print(f"+{ '-' * 45 }+{ '-' * 41 }+")

        # We will render up to 9 rows (4 asks, 1 spread, 4 bids)
        for i in range(9):
            left_col = ""
            right_col = ""

            if i < 4:
                # Asks section (i=0 -> ask3, i=1 -> ask2, i=2 -> ask1, i=3 -> ask0)
                ask_idx = 3 - i
                if ask_idx < len(sorted_asks):
                    price, qty = sorted_asks[ask_idx]
                    bar_len = min(12, max(1, int((qty / max_qty) * 12)))
                    bar = "#" * bar_len
                    left_col = f"  {COLOR_RED}{price:8.2f}{COLOR_RESET}   {qty:<6}  [{COLOR_RED}{bar:<12}{COLOR_RESET}]      "
                else:
                    left_col = "  .          .       .                       "
            elif i == 4:
                # Spread Divider
                left_col = f"{spread_color}{spread_line}{COLOR_RESET}"
            else:
                # Bids section (i=5 -> bid0, i=6 -> bid1, i=7 -> bid2, i=8 -> bid3)
                bid_idx = i - 5
                if bid_idx < len(sorted_bids):
                    price, qty = sorted_bids[bid_idx]
                    bar_len = min(12, max(1, int((qty / max_qty) * 12)))
                    bar = "#" * bar_len
                    left_col = f"  {COLOR_GREEN}{price:8.2f}{COLOR_RESET}   {qty:<6}  [{COLOR_GREEN}{bar:<12}{COLOR_RESET}]      "
                else:
                    left_col = "  .          .       .                       "

            # Right column: Show user orders
            uo_idx = i if i < 4 else (i - 1 if i > 4 else -1)
            
            if uo_idx != -1 and uo_idx < len(user_orders):
                uo = user_orders[uo_idx]
                side_color = COLOR_GREEN if uo["side"] == "BUY" else COLOR_RED
                right_col = f"  {uo['cl_ord_id']:<8}  {side_color}{uo['side']:<4}{COLOR_RESET}  {uo['price']:7.2f}  {uo['leaves_qty']:<4} {COLOR_CYAN}{uo['status']:<8}{COLOR_RESET} "
            elif uo_idx == -1:
                # Spread separator on the right
                right_col = "-" * 41
            else:
                right_col = "  .         .     .      .    .         "

            print(f"|{left_col}|{right_col}|")

        print(f"+{ '-' * 45 }+{ '-' * 41 }+")

        # 3. Recent Trades Box (Width 87 inner)
        print(f"+{ '-' * 87 }+")
        print("|" + " " * 32 + f"{COLOR_BOLD}RECENT MATCHES / TRADES{COLOR_RESET}" + " " * 32 + "|")
        print(f"+{ '-' * 87 }+")
        recent_trades = self.trades[-4:] # last 4 trades
        if not recent_trades:
            print("|   No trades printed yet. Submit matching orders to generate fills.           |")
        else:
            for t in recent_trades:
                print(f"| {COLOR_YELLOW}{t:<85}{COLOR_RESET} |")
        print(f"+{ '-' * 87 }+")

        # 4. System log messages
        print(f"+{ '-' * 87 }+")
        print("|" + " " * 25 + f"{COLOR_GRAY}System Activity Log (Last 3 events){COLOR_RESET}" + " " * 26 + "|")
        print(f"+{ '-' * 87 }+")
        for l in self.log_messages[-3:]:
            raw_l = l.replace(COLOR_GREEN, "").replace(COLOR_RED, "").replace(COLOR_RESET, "").replace(COLOR_CYAN, "").replace(COLOR_BOLD, "").replace(COLOR_GRAY, "")
            padding = " " * (85 - len(raw_l))
            print(f"| {COLOR_GRAY}{l}{padding}{COLOR_RESET} |")
        print(f"+{ '-' * 87 }+")

        # 5. Usage Prompt
        print(f"\n {COLOR_BOLD}COMMANDS:{COLOR_RESET} buy/sell <qty> <price>  |  cancel <id>  |  switch <symbol>  |  r (refresh)  |  q (quit)")
        print(f" {COLOR_GRAY}Example: buy 50 100.50 (sends buy order for active symbol RELIANCE at 100.50){COLOR_RESET}")
        print(f"\n{COLOR_BOLD}Command ({self.active_symbol})>{COLOR_RESET} {input_buffer}", end="", flush=True)

def print_help():
    print("\n--- Command Guide ---")
    print("  buy <qty> <price>      : Places a BUY limit order on the ACTIVE symbol.")
    print("  sell <qty> <price>     : Places a SELL limit order on the ACTIVE symbol.")
    print("  cancel <id>            : Cancels the specified user order ID (e.g. USR-0001).")
    print("  switch <symbol>        : Switches the displayed order book (RELIANCE / INFY / TCS).")
    print("  r                      : Refresh the screen and pull latest updates.")
    print("  q / quit               : Exit the TUI client.")
    print("----------------------\n")
    input("Press Enter to return...")

def get_input_non_blocking(input_buffer):
    if not HAS_MSVCRT:
        return False, False, input_buffer
    if msvcrt.kbhit():
        ch = msvcrt.getch()
        if ch in (b'\r', b'\n'): # Enter
            return True, True, input_buffer
        elif ch == b'\x08': # Backspace
            if len(input_buffer) > 0:
                input_buffer = input_buffer[:-1]
            return True, False, input_buffer
        elif ch == b'\x03': # Ctrl+C
            raise KeyboardInterrupt()
        elif ch in (b'\x00', b'\xe0'): # Special/arrow keys
            if msvcrt.kbhit():
                msvcrt.getch() # Consume next byte
            return False, False, input_buffer
        else:
            try:
                char_str = ch.decode('utf-8', errors='ignore')
                if char_str and char_str.isprintable():
                    input_buffer += char_str
                    return True, False, input_buffer
            except Exception:
                pass
    return False, False, input_buffer

def main():
    client_id = "CLIENT"
    if len(sys.argv) > 1:
        client_id = sys.argv[1]
    client = FixTuiClient(client_id=client_id)
    try:
        client.connect()
    except Exception:
        print("[ERROR] Could not connect to FIX server on 127.0.0.1:5001. Ensure C++ exchange_fix.exe is running first!")
        input("\nPress Enter to exit...")
        return

    # Perform FIX Logon
    client.send_message("A", [(98, 0), (108, 30), (141, "Y")])
    time.sleep(0.5)

    # Pre-populate resting orders for the demo
    client.pre_populate_books()
    time.sleep(0.5)

    # Reset dirty flag after initial setup
    client.dirty = False
    
    input_buffer = ""
    client.render_tui(input_buffer)

    use_non_blocking = HAS_MSVCRT and sys.stdin.isatty()

    while client.running:
        try:
            if use_non_blocking:
                # Check background changes first
                if client.dirty:
                    client.render_tui(input_buffer)
                    client.dirty = False
                
                # Poll input
                char_added, enter_pressed, input_buffer = get_input_non_blocking(input_buffer)
                if char_added:
                    client.render_tui(input_buffer)
                
                if enter_pressed:
                    cmd_line = input_buffer.strip()
                    input_buffer = ""
                    if cmd_line:
                        parts = cmd_line.split()
                        cmd = parts[0].lower()
                        if cmd in ("q", "quit", "exit"):
                            break
                        elif cmd == "r":
                            client.render_tui(input_buffer)
                        elif cmd in ("h", "help"):
                            print_help()
                            client.render_tui(input_buffer)
                        elif cmd == "switch":
                            if len(parts) < 2:
                                client.log("Error: Specify symbol (e.g. switch INFY)")
                            else:
                                sym = parts[1].upper()
                                client.active_symbol = sym
                                client.log(f"Switched view to {sym}")
                            client.render_tui(input_buffer)
                        elif cmd in ("buy", "sell"):
                            if len(parts) < 3:
                                client.log("Error: Format is buy/sell <qty> <price>")
                                client.render_tui(input_buffer)
                                continue
                            try:
                                qty = int(parts[1])
                                price = float(parts[2])
                                side = "BUY" if cmd == "buy" else "SELL"
                                client.submit_user_order(side, client.active_symbol, qty, price)
                                # Redraw is automatically triggered when FIX reports arrive
                            except ValueError:
                                client.log("Error: Quantity must be integer, price must be float.")
                                client.render_tui(input_buffer)
                        elif cmd == "cancel":
                            if len(parts) < 2:
                                client.log("Error: Format is cancel <id>")
                                client.render_tui(input_buffer)
                                continue
                            order_id = parts[1].upper()
                            client.cancel_order(order_id)
                        else:
                            client.log(f"Error: Unknown command '{cmd}'. Type 'h' for help.")
                            client.render_tui(input_buffer)
                    else:
                        client.render_tui(input_buffer)
                
                # Sleep briefly to avoid high CPU usage
                time.sleep(0.01)
            else:
                # Fallback to standard blocking input if msvcrt not available
                cmd_line = input(f"\n{COLOR_BOLD}Command ({client.active_symbol})>{COLOR_RESET} ").strip()
                if not cmd_line:
                    client.render_tui()
                    continue

                parts = cmd_line.split()
                cmd = parts[0].lower()

                if cmd in ("q", "quit", "exit"):
                    break
                elif cmd == "r":
                    client.render_tui()
                elif cmd in ("h", "help"):
                    print_help()
                    client.render_tui()
                elif cmd == "switch":
                    if len(parts) < 2:
                        client.log("Error: Specify symbol (e.g. switch INFY)")
                    else:
                        sym = parts[1].upper()
                        client.active_symbol = sym
                        client.log(f"Switched view to {sym}")
                    client.render_tui()
                elif cmd in ("buy", "sell"):
                    if len(parts) < 3:
                        client.log("Error: Format is buy/sell <qty> <price>")
                        client.render_tui()
                        continue
                    try:
                        qty = int(parts[1])
                        price = float(parts[2])
                        side = "BUY" if cmd == "buy" else "SELL"
                        client.submit_user_order(side, client.active_symbol, qty, price)
                        time.sleep(0.15)
                        client.render_tui()
                    except ValueError:
                        client.log("Error: Quantity must be integer, price must be float.")
                        client.render_tui()
                elif cmd == "cancel":
                    if len(parts) < 2:
                        client.log("Error: Format is cancel <id>")
                        client.render_tui()
                        continue
                    order_id = parts[1].upper()
                    client.cancel_order(order_id)
                    time.sleep(0.15)
                    client.render_tui()
                else:
                    client.log(f"Error: Unknown command '{cmd}'. Type 'h' for help.")
                    client.render_tui()
        except KeyboardInterrupt:
            break
        except Exception as e:
            client.log(f"Error: {e}")
            if use_non_blocking:
                client.render_tui(input_buffer)
            else:
                client.render_tui()

    client.disconnect()

if __name__ == "__main__":
    main()
