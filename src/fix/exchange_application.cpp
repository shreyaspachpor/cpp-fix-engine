#include "fix/exchange_application.h"

#include <quickfix/Session.h>
#include <iostream>
#include <sstream>

// ── Constructor ───────────────────────────────────────────────────────────────

ExchangeApplication::ExchangeApplication(Exchange& exchange, const RiskContext& risk_ctx)
    : exchange_(exchange), risk_ctx_(risk_ctx)
{
    std::cout << std::unitbuf;
}

// ── Session callbacks ─────────────────────────────────────────────────────────

void ExchangeApplication::onLogon(const FIX::SessionID& session_id)
{
    std::cout << "[FIX] Client logged on: " << session_id << "\n";
    std::lock_guard<std::mutex> lock(global_state_mutex_);
    active_sessions_.insert(session_id);
}

void ExchangeApplication::onLogout(const FIX::SessionID& session_id)
{
    std::cout << "[FIX] Client logged out: " << session_id << "\n";
    std::lock_guard<std::mutex> lock(global_state_mutex_);
    active_sessions_.erase(session_id);
}

void ExchangeApplication::toAdmin(FIX::Message& msg, const FIX::SessionID& session_id)
{
    FIX::MsgType msgType;
    if (msg.getHeader().getFieldIfSet(msgType))
    {
        if (msgType.getValue() == "0")
        {
            std::cout << "[FIX] Sending Heartbeat to client...\n";
        }
    }
}

void ExchangeApplication::fromAdmin(const FIX::Message& msg, const FIX::SessionID& session_id)
{
    FIX::MsgType msgType;
    if (msg.getHeader().getFieldIfSet(msgType))
    {
        if (msgType.getValue() == "0")
        {
            std::cout << "[FIX] Received Heartbeat from client...\n";
        }
    }
}

void ExchangeApplication::fromApp(const FIX::Message& msg, const FIX::SessionID& session_id)
{
    // MessageCracker::crack() dispatches to the correct typed onMessage() overload.
    crack(msg, session_id);
}

std::shared_ptr<std::mutex> ExchangeApplication::get_mutex_for_symbol(const std::string& symbol)
{
    std::lock_guard<std::mutex> lock(mutex_map_guard_);
    auto it = symbol_mutexes_.find(symbol);
    if (it == symbol_mutexes_.end())
    {
        auto sym_mutex = std::make_shared<std::mutex>();
        symbol_mutexes_[symbol] = sym_mutex;
        return sym_mutex;
    }
    return it->second;
}

// ── Inbound: NewOrderSingle (D) ───────────────────────────────────────────────

void ExchangeApplication::onMessage(const FIX42::NewOrderSingle& msg,
                                     const FIX::SessionID& session_id)
{
    FIX::ClOrdID   cl_ord_id_field;
    FIX::Symbol    symbol_field;
    FIX::Side      side_field;
    FIX::OrdType   ord_type_field;
    FIX::Price     price_field;
    FIX::OrderQty  order_qty_field;

    msg.get(cl_ord_id_field);
    msg.get(symbol_field);
    msg.get(side_field);
    msg.get(ord_type_field);
    msg.get(price_field);
    msg.get(order_qty_field);

    const std::string cl_ord_id = cl_ord_id_field.getValue();
    const std::string symbol    = symbol_field.getValue();
    const char        side_char = side_field.getValue();
    const double      price     = price_field.getValue();
    const int         qty       = static_cast<int>(order_qty_field.getValue());

    std::cout << "[FIX] Inbound Order -> ID: " << cl_ord_id 
              << ", Symbol: " << symbol 
              << ", Side: " << (side_char == '1' ? "BUY" : "SELL")
              << ", Qty: " << qty 
              << ", Price: " << price << "\n";

    // Reject non-limit orders (we only support Limit for now)
    if (ord_type_field.getValue() != FIX::OrdType_LIMIT)
    {
        OrderId oid;
        {
            std::lock_guard<std::mutex> lock(global_state_mutex_);
            oid = next_order_id_++;
            clordid_map_[cl_ord_id] = oid;
            orderid_to_clordid_map_[oid] = cl_ord_id;
            order_sessions_[oid] = session_id;
        }
        OrderState rejected_state(oid, symbol, qty, price,
                                  side_char == '1' ? Side::Buy : Side::Sell,
                                  OrderType::Limit);
        rejected_state.order_status = OrderStatus::Rejected;
        std::cout << "[FIX] Order " << cl_ord_id << " Rejected: Only Limit orders supported\n";
        send_exec_report(session_id, cl_ord_id, oid, rejected_state,
                         ExecType::Rejected, "Only Limit orders supported");
        return;
    }

    OrderId order_id;
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        order_id = next_order_id_++;
        clordid_map_[cl_ord_id] = order_id;
        orderid_to_clordid_map_[order_id] = cl_ord_id;
        order_sessions_[order_id] = session_id;
    }

    Side side = (side_char == FIX::Side_BUY) ? Side::Buy : Side::Sell;
    Order order;
    order.order_id   = order_id;
    order.price      = price;
    order.symbol     = symbol;
    order.side       = side;
    order.order_type = OrderType::Limit;
    order.quantity   = qty;

    std::shared_ptr<std::mutex> symbol_mutex = get_mutex_for_symbol(symbol);
    std::vector<Trade> trades;
    OrderState state;

    {
        std::lock_guard<std::mutex> sym_lock(*symbol_mutex);
        trades = exchange_.submit_order(order, risk_ctx_);
        state = exchange_.get_order_state(order_id);
    }

    if (state.order_status == OrderStatus::Rejected)
    {
        std::cout << "[FIX] Order " << cl_ord_id << " Rejected: Risk check failed\n";
        send_exec_report(session_id, cl_ord_id, order_id, state,
                         ExecType::Rejected, "Risk check failed");
        return;
    }

    // Send New ack first
    send_exec_report(session_id, cl_ord_id, order_id, state, ExecType::New);

    // Send fill reports for any trades generated
    for (const auto& t : trades)
    {
        OrderState updated;
        {
            std::lock_guard<std::mutex> sym_lock(*symbol_mutex);
            updated = exchange_.get_order_state(order_id);
        }

        char exec_type = (updated.order_status == OrderStatus::Filled)
                         ? ExecType::Fill : ExecType::PartialFill;
        send_exec_report(session_id, cl_ord_id, order_id, updated, exec_type, t.quantity, t.price);

        // Report for the resting order (which was filled by this match)
        OrderId resting_order_id = (order_id == t.buy_order_id) ? t.sell_order_id : t.buy_order_id;
        
        std::string resting_cl_ord_id = "UNKNOWN";
        FIX::SessionID resting_session = session_id;
        {
            std::lock_guard<std::mutex> lock(global_state_mutex_);
            auto rest_it = orderid_to_clordid_map_.find(resting_order_id);
            if (rest_it != orderid_to_clordid_map_.end())
                resting_cl_ord_id = rest_it->second;

            auto session_it = order_sessions_.find(resting_order_id);
            if (session_it != order_sessions_.end())
                resting_session = session_it->second;
        }

        std::cout << "[FIX] Match Execution -> " << t.quantity 
                  << " shares of " << symbol 
                  << " matched @ " << t.price 
                  << " (Buy ID: " << (order_id == t.buy_order_id ? cl_ord_id : resting_cl_ord_id) 
                  << ", Sell ID: " << (order_id == t.sell_order_id ? cl_ord_id : resting_cl_ord_id) << ")\n";

        if (resting_cl_ord_id != "UNKNOWN")
        {
            OrderState updated_resting;
            {
                std::lock_guard<std::mutex> sym_lock(*symbol_mutex);
                updated_resting = exchange_.get_order_state(resting_order_id);
            }
            char exec_type_rest = (updated_resting.order_status == OrderStatus::Filled)
                                  ? ExecType::Fill : ExecType::PartialFill;
            send_exec_report(resting_session, resting_cl_ord_id, resting_order_id, updated_resting, exec_type_rest, t.quantity, t.price);
        }
    }
}

// ── Inbound: OrderCancelRequest (F) ──────────────────────────────────────────

void ExchangeApplication::onMessage(const FIX42::OrderCancelRequest& msg,
                                     const FIX::SessionID& session_id)
{
    FIX::ClOrdID      cl_ord_id_field;
    FIX::OrigClOrdID  orig_cl_ord_id_field;
    FIX::Symbol       symbol_field;

    msg.get(cl_ord_id_field);
    msg.get(orig_cl_ord_id_field);
    msg.get(symbol_field);

    const std::string cl_ord_id      = cl_ord_id_field.getValue();
    const std::string orig_cl_ord_id = orig_cl_ord_id_field.getValue();
    const std::string symbol         = symbol_field.getValue();

    std::cout << "[FIX] Inbound Cancel Request -> Target ClOrdID: " << orig_cl_ord_id 
              << ", New ClOrdID: " << cl_ord_id << ", Symbol: " << symbol << "\n";

    OrderId order_id = 0;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        auto it = clordid_map_.find(orig_cl_ord_id);
        if (it != clordid_map_.end())
        {
            order_id = it->second;
            found = true;
        }
    }

    if (!found)
    {
        std::cout << "[FIX] Cancel Rejected for ClOrdID: " << orig_cl_ord_id << " (Reason: Unknown ClOrdID)\n";
        send_cancel_reject(session_id, cl_ord_id, orig_cl_ord_id, "Unknown ClOrdID");
        return;
    }

    std::shared_ptr<std::mutex> symbol_mutex = get_mutex_for_symbol(symbol);
    std::lock_guard<std::mutex> sym_lock(*symbol_mutex);

    OrderState state_before = exchange_.get_order_state(order_id);

    if (state_before.is_terminal())
    {
        std::cout << "[FIX] Cancel Rejected for ClOrdID: " << orig_cl_ord_id << " (Reason: Order already in terminal state)\n";
        send_cancel_reject(session_id, cl_ord_id, orig_cl_ord_id,
                           "Order already in terminal state");
        return;
    }

    exchange_.cancel_order(order_id);

    OrderState state = exchange_.get_order_state(order_id);
    
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        clordid_map_[cl_ord_id] = order_id;
        orderid_to_clordid_map_[order_id] = cl_ord_id;
    }

    std::cout << "[FIX] Order " << orig_cl_ord_id << " Cancelled successfully\n";
    send_exec_report(session_id, cl_ord_id, order_id, state, ExecType::Cancelled);
}

// ── Inbound: OrderCancelReplaceRequest (G) ────────────────────────────────────

void ExchangeApplication::onMessage(const FIX42::OrderCancelReplaceRequest& msg,
                                     const FIX::SessionID& session_id)
{
    FIX::ClOrdID      cl_ord_id_field;
    FIX::OrigClOrdID  orig_cl_ord_id_field;
    FIX::Symbol       symbol_field;
    FIX::Price        price_field;
    FIX::OrderQty     order_qty_field;

    msg.get(cl_ord_id_field);
    msg.get(orig_cl_ord_id_field);
    msg.get(symbol_field);
    msg.get(price_field);
    msg.get(order_qty_field);

    const std::string cl_ord_id      = cl_ord_id_field.getValue();
    const std::string orig_cl_ord_id = orig_cl_ord_id_field.getValue();
    const std::string symbol         = symbol_field.getValue();
    const double      new_price      = price_field.getValue();
    const int         new_qty        = static_cast<int>(order_qty_field.getValue());

    std::cout << "[FIX] Inbound Replace Request -> Target ClOrdID: " << orig_cl_ord_id 
              << ", New Qty: " << new_qty << ", New Price: " << new_price << ", New ClOrdID: " << cl_ord_id << "\n";

    OrderId order_id = 0;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        auto it = clordid_map_.find(orig_cl_ord_id);
        if (it != clordid_map_.end())
        {
            order_id = it->second;
            found = true;
        }
    }

    if (!found)
    {
        std::cout << "[FIX] Replace Rejected for ClOrdID: " << orig_cl_ord_id << " (Reason: Unknown ClOrdID)\n";
        send_cancel_reject(session_id, cl_ord_id, orig_cl_ord_id, "Unknown ClOrdID");
        return;
    }

    std::shared_ptr<std::mutex> symbol_mutex = get_mutex_for_symbol(symbol);
    std::vector<Trade> trades;
    OrderState state;

    {
        std::lock_guard<std::mutex> sym_lock(*symbol_mutex);

        OrderState state_before = exchange_.get_order_state(order_id);
        if (state_before.is_terminal())
        {
            std::cout << "[FIX] Replace Rejected for ClOrdID: " << orig_cl_ord_id << " (Reason: Order already in terminal state)\n";
            send_cancel_reject(session_id, cl_ord_id, orig_cl_ord_id,
                               "Order already in terminal state");
            return;
        }

        trades = exchange_.modify_order(order_id, new_price, new_qty, risk_ctx_);
        state = exchange_.get_order_state(order_id);
    }

    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        clordid_map_[cl_ord_id] = order_id;
        orderid_to_clordid_map_[order_id] = cl_ord_id;
        order_sessions_[order_id] = session_id;
    }

    std::cout << "[FIX] Order " << orig_cl_ord_id << " Replaced successfully\n";
    send_exec_report(session_id, cl_ord_id, order_id, state, ExecType::Replace);

    // Send fill reports for any immediate matches after the replace
    for (const auto& t : trades)
    {
        OrderState updated;
        {
            std::lock_guard<std::mutex> sym_lock(*symbol_mutex);
            updated = exchange_.get_order_state(order_id);
        }

        char exec_type = (updated.order_status == OrderStatus::Filled)
                         ? ExecType::Fill : ExecType::PartialFill;
        send_exec_report(session_id, cl_ord_id, order_id, updated, exec_type, t.quantity, t.price);

        // Report for the resting order (which was filled by this match)
        OrderId resting_order_id = (order_id == t.buy_order_id) ? t.sell_order_id : t.buy_order_id;
        
        std::string resting_cl_ord_id = "UNKNOWN";
        FIX::SessionID resting_session = session_id;
        {
            std::lock_guard<std::mutex> lock(global_state_mutex_);
            auto rest_it = orderid_to_clordid_map_.find(resting_order_id);
            if (rest_it != orderid_to_clordid_map_.end())
                resting_cl_ord_id = rest_it->second;

            auto session_it = order_sessions_.find(resting_order_id);
            if (session_it != order_sessions_.end())
                resting_session = session_it->second;
        }

        std::cout << "[FIX] Match Execution -> " << t.quantity 
                  << " shares matched @ " << t.price 
                  << " (Buy ID: " << (order_id == t.buy_order_id ? cl_ord_id : resting_cl_ord_id) 
                  << ", Sell ID: " << (order_id == t.sell_order_id ? cl_ord_id : resting_cl_ord_id) << ")\n";

        if (resting_cl_ord_id != "UNKNOWN")
        {
            OrderState updated_resting;
            {
                std::lock_guard<std::mutex> sym_lock(*symbol_mutex);
                updated_resting = exchange_.get_order_state(resting_order_id);
            }
            char exec_type_rest = (updated_resting.order_status == OrderStatus::Filled)
                                  ? ExecType::Fill : ExecType::PartialFill;
            send_exec_report(resting_session, resting_cl_ord_id, resting_order_id, updated_resting, exec_type_rest, t.quantity, t.price);
        }
    }
}

// ── Outbound helpers ──────────────────────────────────────────────────────────

void ExchangeApplication::send_exec_report(const FIX::SessionID& session_id,
                                            const std::string&    cl_ord_id,
                                            OrderId               order_id,
                                            const OrderState&     state,
                                            char                  exec_type,
                                            const std::string&    text)
{
    uint64_t exec_id_val;
    std::vector<FIX::SessionID> targets;
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        exec_id_val = next_exec_id_++;
        for (const auto& s : active_sessions_)
            targets.push_back(s);
    }
    if (targets.empty())
        targets.push_back(session_id);

    std::string exec_id = "EXEC-" + std::to_string(exec_id_val);

    FIX42::ExecutionReport report(
        FIX::OrderID    (std::to_string(order_id)),
        FIX::ExecID     (exec_id),
        FIX::ExecTransType(FIX::ExecTransType_NEW),
        FIX::ExecType   (exec_type),
        FIX::OrdStatus  (order_status_char(state.order_status)),
        FIX::Symbol     (state.symbol),
        FIX::Side       (state.side == Side::Buy ? FIX::Side_BUY : FIX::Side_SELL),
        FIX::LeavesQty  (state.remaining_quantity()),
        FIX::CumQty     (state.filled_quantity),
        FIX::AvgPx      (state.price)
    );

    report.set(FIX::ClOrdID (cl_ord_id));
    report.set(FIX::OrderQty(state.original_quantity));
    report.set(FIX::Price   (state.price));

    if (!text.empty())
        report.set(FIX::Text(text));

    for (const auto& target : targets)
    {
        FIX::Session::sendToTarget(report, target);
    }
}

void ExchangeApplication::send_exec_report(const FIX::SessionID& session_id,
                                            const std::string&    cl_ord_id,
                                            OrderId               order_id,
                                            const OrderState&     state,
                                            char                  exec_type,
                                            Quantity              last_shares,
                                            Price                 last_px)
{
    uint64_t exec_id_val;
    std::vector<FIX::SessionID> targets;
    {
        std::lock_guard<std::mutex> lock(global_state_mutex_);
        exec_id_val = next_exec_id_++;
        for (const auto& s : active_sessions_)
            targets.push_back(s);
    }
    if (targets.empty())
        targets.push_back(session_id);

    std::string exec_id = "EXEC-" + std::to_string(exec_id_val);

    FIX42::ExecutionReport report(
        FIX::OrderID    (std::to_string(order_id)),
        FIX::ExecID     (exec_id),
        FIX::ExecTransType(FIX::ExecTransType_NEW),
        FIX::ExecType   (exec_type),
        FIX::OrdStatus  (order_status_char(state.order_status)),
        FIX::Symbol     (state.symbol),
        FIX::Side       (state.side == Side::Buy ? FIX::Side_BUY : FIX::Side_SELL),
        FIX::LeavesQty  (state.remaining_quantity()),
        FIX::CumQty     (state.filled_quantity),
        FIX::AvgPx      (state.price)
    );

    report.set(FIX::ClOrdID (cl_ord_id));
    report.set(FIX::OrderQty(state.original_quantity));
    report.set(FIX::Price   (state.price));

    if (last_shares > 0)
    {
        report.set(FIX::LastShares(last_shares));
        report.set(FIX::LastPx(last_px));
    }

    for (const auto& target : targets)
    {
        FIX::Session::sendToTarget(report, target);
    }
}

void ExchangeApplication::send_cancel_reject(const FIX::SessionID& session_id,
                                              const std::string&    cl_ord_id,
                                              const std::string&    orig_cl_ord_id,
                                              const std::string&    reason)
{
    FIX42::OrderCancelReject reject(
        FIX::OrderID   ("NONE"),
        FIX::ClOrdID   (cl_ord_id),
        FIX::OrigClOrdID(orig_cl_ord_id),
        FIX::OrdStatus (FIX::OrdStatus_REJECTED),
        FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST)
    );
    reject.set(FIX::Text(reason));
    FIX::Session::sendToTarget(reject, session_id);
}

char ExchangeApplication::order_status_char(OrderStatus s) const
{
    switch (s)
    {
        case OrderStatus::New:            return OrdStatus::New;
        case OrderStatus::Partial_Filled: return OrdStatus::PartialFill;
        case OrderStatus::Filled:         return OrdStatus::Fill;
        case OrderStatus::Cancelled:      return OrdStatus::Cancelled;
        case OrderStatus::Rejected:       return OrdStatus::Rejected;
        default:                          return OrdStatus::Rejected;
    }
}
