import socket
import datetime
import time
import threading

class PureFixClient:
    def __init__(self, host="127.0.0.1", port=5001):
        self.host = host
        self.port = port
        self.sock = None
        self.seq_num = 1
        self.running = False
        self.buffer = b""
        self.recv_thread = None

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.running = True
        self.recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self.recv_thread.start()
        print(f"[CLIENT] Connected to {self.host}:{self.port}")

    def disconnect(self):
        self.running = False
        if self.sock:
            self.sock.close()
        print("[CLIENT] Disconnected")

    def _recv_loop(self):
        while self.running:
            try:
                data = self.sock.recv(4096)
                if not data:
                    break
                self.buffer += data
                self._parse_buffer()
            except Exception as e:
                break

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
        readable = msg_str.replace("\x01", "|")
        fields = {}
        for item in msg_str.split("\x01"):
            if "=" in item:
                k, v = item.split("=", 1)
                fields[k] = v

        msg_type = fields.get("35")
        if msg_type == "A":
            print(f"[RECV LOGON] {readable}")
        elif msg_type == "0":
            print(f"[RECV HEARTBEAT] {readable}")
        elif msg_type == "8":
            cl_ord_id = fields.get("11", "")
            exec_type = fields.get("150", "")
            ord_status = fields.get("39", "")
            leaves_qty = fields.get("151", "0")
            cum_qty = fields.get("14", "0")
            price = fields.get("44", "0.00")
            symbol = fields.get("55", "")

            status_map = {'0':'New','1':'PartialFill','2':'Filled','4':'Cancelled','8':'Rejected'}
            etype_map  = {'0':'New','1':'PartialFill','2':'Fill',  '4':'Cancelled','5':'Replace', '8':'Rejected'}

            print(f"[RECV EXEC REPORT] ClOrdID={cl_ord_id}  Symbol={symbol}"
                  f"  ExecType={etype_map.get(exec_type, exec_type)}"
                  f"  Status={status_map.get(ord_status, ord_status)}"
                  f"  CumQty={cum_qty}"
                  f"  LeavesQty={leaves_qty}"
                  f"  Price={price}")
        elif msg_type == "9":
            cl_ord_id = fields.get("11", "")
            orig_cl_ord_id = fields.get("41", "")
            reason = fields.get("58", "")
            print(f"[RECV CANCEL REJECT] ClOrdID={cl_ord_id}  OrigClOrdID={orig_cl_ord_id}  Reason={reason}")
        else:
            print(f"[RECV MSG TYPE {msg_type}] {readable}")

    def send_message(self, msg_type, fields):
        sending_time = datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S.%f")[:-3]
        body_fields = [
            (35, msg_type),
            (49, "CLIENT"),
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
        readable = full_msg.decode('ascii').replace("\x01", "|")
        print(f"[SEND] {readable}")

def main():
    client = PureFixClient()
    client.connect()

    # 1. Logon
    print("\n--- Step 1: Logon ---")
    client.send_message("A", [(98, 0), (108, 30), (141, "Y")])
    time.sleep(0.5)

    # 2. RELIANCE Sell 100 @ 100.00
    print("\n--- Step 2: RELIANCE SELL 100 @ 100.00 ---")
    client.send_message("D", [
        (11, "ORD-0001"),
        (21, 1),
        (55, "RELIANCE"),
        (54, 2),  # Sell
        (40, 2),  # Limit
        (44, "100.00"),
        (38, 100),
        (59, 0),  # Day
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 3. RELIANCE Buy 50 @ 100.00
    print("\n--- Step 3: RELIANCE BUY 50 @ 100.00 ---")
    client.send_message("D", [
        (11, "ORD-0002"),
        (21, 1),
        (55, "RELIANCE"),
        (54, 1),  # Buy
        (40, 2),  # Limit
        (44, "100.00"),
        (38, 50),
        (59, 0),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 4. Cancel remaining RELIANCE Sell
    print("\n--- Step 4: Cancel RELIANCE sell ---")
    client.send_message("F", [
        (11, "ORD-0003"),
        (41, "ORD-0001"),
        (55, "RELIANCE"),
        (54, 2),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 5. INFY Sell 60 @ 1500.00
    print("\n--- Step 5: INFY SELL 60 @ 1500.00 ---")
    client.send_message("D", [
        (11, "ORD-0004"),
        (21, 1),
        (55, "INFY"),
        (54, 2),
        (40, 2),
        (44, "1500.00"),
        (38, 60),
        (59, 0),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 6. Modify INFY Sell -> 40 @ 1510.00
    print("\n--- Step 6: Modify INFY sell -> 40 @ 1510.00 ---")
    client.send_message("G", [
        (11, "ORD-0005"),
        (41, "ORD-0004"),
        (55, "INFY"),
        (54, 2),
        (40, 2),
        (44, "1510.00"),
        (38, 40),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 7. INFY Buy 40 @ 1510.00
    print("\n--- Step 7: INFY BUY 40 @ 1510.00 (fills modified sell) ---")
    client.send_message("D", [
        (11, "ORD-0006"),
        (21, 1),
        (55, "INFY"),
        (54, 1),
        (40, 2),
        (44, "1510.00"),
        (38, 40),
        (59, 0),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    # 8. Cancel already-cancelled order
    print("\n--- Step 8: Cancel already-cancelled RELIANCE sell (expect reject) ---")
    client.send_message("F", [
        (11, "ORD-0007"),
        (41, "ORD-0001"),
        (55, "RELIANCE"),
        (54, 2),
        (60, datetime.datetime.utcnow().strftime("%Y%m%d-%H:%M:%S"))
    ])
    time.sleep(0.5)

    print("\n[CLIENT] Waiting for any final reports...")
    time.sleep(1.0)
    client.disconnect()

if __name__ == "__main__":
    main()
