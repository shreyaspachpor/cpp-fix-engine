"""
FIX 4.2 test client for CM Trading System
Requires: pip install quickfix

Usage:
    python fix_client.py

The client connects to localhost:5001 and runs a scripted sequence:
  1. NewOrderSingle  — RELIANCE sell  100 @ 100.00
  2. NewOrderSingle  — RELIANCE buy    50 @ 100.00  (partial fill on #1)
  3. OrderCancelRequest            — cancel #1 (remaining 50)
  4. NewOrderSingle  — INFY sell    60 @ 1500.00
  5. OrderCancelReplaceRequest     — modify #4 → 40 @ 1510.00
  6. NewOrderSingle  — INFY buy     40 @ 1510.00  (fills modified #4)
"""

import quickfix as fix
import quickfix42 as fix42
import time
import threading

class TestClient(fix.Application):
    def __init__(self):
        super().__init__()
        self.session_id = None
        self.order_seq  = 1   # ClOrdID counter
        self._ready     = threading.Event()

    # ── Session callbacks ─────────────────────────────────────────────────────

    def onCreate(self, session_id):
        pass

    def onLogon(self, session_id):
        print(f"[CLIENT] Logged on: {session_id}")
        self.session_id = session_id
        self._ready.set()

    def onLogout(self, session_id):
        print(f"[CLIENT] Logged out: {session_id}")

    def toAdmin(self, msg, session_id):
        pass

    def fromAdmin(self, msg, session_id):
        pass

    def toApp(self, msg, session_id):
        print(f"[SEND] {msg.toString().replace(chr(1), '|')}")

    def fromApp(self, msg, session_id):
        msg_type = fix.MsgType()
        msg.getHeader().getField(msg_type)

        if msg_type.getValue() == fix.MsgType_ExecutionReport:
            self._on_exec_report(msg)
        elif msg_type.getValue() == fix.MsgType_OrderCancelReject:
            self._on_cancel_reject(msg)

    # ── Message handlers ──────────────────────────────────────────────────────

    def _on_exec_report(self, msg):
        cl_ord_id = fix.ClOrdID();    msg.getField(cl_ord_id)
        exec_type = fix.ExecType();   msg.getField(exec_type)
        ord_status= fix.OrdStatus();  msg.getField(ord_status)
        leaves_qty= fix.LeavesQty();  msg.getField(leaves_qty)
        cum_qty   = fix.CumQty();     msg.getField(cum_qty)

        status_map = {'0':'New','1':'PartialFill','2':'Filled','4':'Cancelled','8':'Rejected'}
        etype_map  = {'0':'New','1':'PartialFill','2':'Fill',  '4':'Cancelled','5':'Replace', '8':'Rejected'}

        print(f"[EXEC REPORT] ClOrdID={cl_ord_id.getValue()}"
              f"  ExecType={etype_map.get(exec_type.getValue(), '?')}"
              f"  Status={status_map.get(ord_status.getValue(), '?')}"
              f"  CumQty={int(cum_qty.getValue())}"
              f"  LeavesQty={int(leaves_qty.getValue())}")

    def _on_cancel_reject(self, msg):
        cl_ord_id = fix.ClOrdID(); msg.getField(cl_ord_id)
        text_field= fix.Text()
        reason = ""
        try:
            msg.getField(text_field)
            reason = text_field.getValue()
        except:
            pass
        print(f"[CANCEL REJECT] ClOrdID={cl_ord_id.getValue()}  Reason: {reason}")

    # ── Order helpers ─────────────────────────────────────────────────────────

    def _next_cl_ord_id(self):
        cid = f"ORD-{self.order_seq:04d}"
        self.order_seq += 1
        return cid

    def send_new_order(self, symbol, side, price, qty):
        cl_ord_id = self._next_cl_ord_id()
        msg = fix42.NewOrderSingle()
        msg.setField(fix.ClOrdID(cl_ord_id))
        msg.setField(fix.Symbol(symbol))
        msg.setField(fix.Side(fix.Side_BUY if side == 'BUY' else fix.Side_SELL))
        msg.setField(fix.OrdType(fix.OrdType_LIMIT))
        msg.setField(fix.Price(price))
        msg.setField(fix.OrderQty(qty))
        msg.setField(fix.TimeInForce(fix.TimeInForce_DAY))
        msg.setField(fix.TransactTime())
        fix.Session.sendToTarget(msg, self.session_id)
        print(f"[SEND NEW]    ClOrdID={cl_ord_id}  {symbol} {side} {qty}@{price}")
        return cl_ord_id

    def send_cancel(self, orig_cl_ord_id, symbol, side):
        cl_ord_id = self._next_cl_ord_id()
        msg = fix42.OrderCancelRequest()
        msg.setField(fix.ClOrdID(cl_ord_id))
        msg.setField(fix.OrigClOrdID(orig_cl_ord_id))
        msg.setField(fix.Symbol(symbol))
        msg.setField(fix.Side(fix.Side_BUY if side == 'BUY' else fix.Side_SELL))
        msg.setField(fix.TransactTime())
        fix.Session.sendToTarget(msg, self.session_id)
        print(f"[SEND CANCEL] ClOrdID={cl_ord_id}  Cancelling {orig_cl_ord_id}")
        return cl_ord_id

    def send_modify(self, orig_cl_ord_id, symbol, side, new_price, new_qty):
        cl_ord_id = self._next_cl_ord_id()
        msg = fix42.OrderCancelReplaceRequest()
        msg.setField(fix.ClOrdID(cl_ord_id))
        msg.setField(fix.OrigClOrdID(orig_cl_ord_id))
        msg.setField(fix.Symbol(symbol))
        msg.setField(fix.Side(fix.Side_BUY if side == 'BUY' else fix.Side_SELL))
        msg.setField(fix.OrdType(fix.OrdType_LIMIT))
        msg.setField(fix.Price(new_price))
        msg.setField(fix.OrderQty(new_qty))
        msg.setField(fix.TransactTime())
        fix.Session.sendToTarget(msg, self.session_id)
        print(f"[SEND MODIFY] ClOrdID={cl_ord_id}  Replacing {orig_cl_ord_id} → {new_qty}@{new_price}")
        return cl_ord_id

    # ── Test script ───────────────────────────────────────────────────────────

    def run_tests(self):
        self._ready.wait(timeout=10)
        if not self.session_id:
            print("[ERROR] Did not connect within 10s")
            return

        time.sleep(0.5)
        print("\n" + "="*60)
        print("  Running FIX test sequence")
        print("="*60 + "\n")

        # 1. RELIANCE sell 100 @ 100
        print("--- Step 1: RELIANCE SELL 100 @ 100.00 ---")
        rel_sell = self.send_new_order("RELIANCE", "SELL", 100.00, 100)
        time.sleep(0.5)

        # 2. RELIANCE buy 50 @ 100 → partial fill on sell
        print("\n--- Step 2: RELIANCE BUY 50 @ 100.00 (partial fill) ---")
        self.send_new_order("RELIANCE", "BUY", 100.00, 50)
        time.sleep(0.5)

        # 3. Cancel remaining RELIANCE sell
        print("\n--- Step 3: Cancel RELIANCE sell ---")
        self.send_cancel(rel_sell, "RELIANCE", "SELL")
        time.sleep(0.5)

        # 4. INFY sell 60 @ 1500
        print("\n--- Step 4: INFY SELL 60 @ 1500.00 ---")
        infy_sell = self.send_new_order("INFY", "SELL", 1500.00, 60)
        time.sleep(0.5)

        # 5. Modify INFY sell → 40 @ 1510
        print("\n--- Step 5: Modify INFY sell → 40 @ 1510.00 ---")
        infy_mod = self.send_modify(infy_sell, "INFY", "SELL", 1510.00, 40)
        time.sleep(0.5)

        # 6. INFY buy 40 @ 1510 → fills modified sell
        print("\n--- Step 6: INFY BUY 40 @ 1510.00 (fills modified sell) ---")
        self.send_new_order("INFY", "BUY", 1510.00, 40)
        time.sleep(0.5)

        # 7. Cancel already-cancelled order (should get OrderCancelReject)
        print("\n--- Step 7: Cancel already-cancelled RELIANCE sell (expect reject) ---")
        self.send_cancel(rel_sell, "RELIANCE", "SELL")
        time.sleep(0.5)

        print("\n" + "="*60)
        print("  Test sequence complete")
        print("="*60)


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    settings_str = """
[DEFAULT]
ConnectionType=initiator
BeginString=FIX.4.2
SenderCompID=CLIENT
TargetCompID=EXCHANGE
HeartBtInt=30
ReconnectInterval=5
FileStorePath=fix_client_store
FileLogPath=fix_client_log
ResetOnLogon=Y
UseDataDictionary=N

[SESSION]
BeginString=FIX.4.2
SocketConnectHost=127.0.0.1
SocketConnectPort=5001
"""
    import tempfile, os
    # Write settings to a temp file
    tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.cfg', delete=False)
    tmp.write(settings_str)
    tmp.close()

    try:
        settings      = fix.SessionSettings(tmp.name)
        application   = TestClient()
        store_factory = fix.FileStoreFactory(settings)
        log_factory   = fix.FileLogFactory(settings)
        initiator     = fix.SocketInitiator(application, store_factory, settings, log_factory)

        initiator.start()
        application.run_tests()

        print("\nPress Enter to disconnect.")
        input()
        initiator.stop()
    finally:
        os.unlink(tmp.name)


if __name__ == "__main__":
    main()
