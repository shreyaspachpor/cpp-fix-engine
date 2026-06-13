
#include "core/oms.h"
#include "core/order_state.h"
#include "core/trade.h"
#include "core/matching_engine.h"
#include "matching/order_book.h"
#include "risk/i_risk_engine.h"
#include "risk/risk_context.h"

OMS::OMS(MatchingEngine& engine, IRiskEngine& risk_engine)
    : engine_(engine), risk_engine_(risk_engine) {}

// ── Internal helper ───────────────────────────────────────────────────────────

void OMS::apply_trade(const Trade& trade)
{
    // Update buy side
    OrderState& buy = order_states_.at(trade.buy_order_id);
    buy.filled_quantity += trade.quantity;
    buy.order_status = (buy.remaining_quantity() == 0)
                       ? OrderStatus::Filled : OrderStatus::Partial_Filled;

    // Update sell side
    OrderState& sell = order_states_.at(trade.sell_order_id);
    sell.filled_quantity += trade.quantity;
    sell.order_status = (sell.remaining_quantity() == 0)
                        ? OrderStatus::Filled : OrderStatus::Partial_Filled;

    trades_.push_back(trade);

    // Bug fix: store Executions (previously created but immediately discarded)
    executions_.push_back({trade.trade_id, trade.buy_order_id,  Side::Buy,  trade.price, trade.quantity});
    executions_.push_back({trade.trade_id, trade.sell_order_id, Side::Sell, trade.price, trade.quantity});
}

// ── Public API ────────────────────────────────────────────────────────────────

std::vector<Trade> OMS::submit_order(const Order& order, const RiskContext& risk_ctx)
{
    // Symbol validation before risk — unknown symbols are immediately rejected
    if (!engine_.has_symbol(order.symbol))
    {
        OrderState state(order.order_id, order.symbol, order.quantity,
                         order.price, order.side, order.order_type);
        state.order_status = OrderStatus::Rejected;
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_.insert({order.order_id, state});
        return {};
    }

    RiskResult result = risk_engine_.check_order(risk_ctx, order);

    OrderState order_state(order.order_id, order.symbol, order.quantity,
                           order.price, order.side, order.order_type);

    if (result.check_type == RiskCheckType::Rejected)
    {
        order_state.order_status = OrderStatus::Rejected;
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_.insert({order.order_id, order_state});
        return {};
    }

    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_.insert({order.order_id, order_state});
    }

    // Pass global next_trade_id_ so IDs are unique across all symbol books
    // process_order runs outside oms_mutex_ for maximum concurrent matching.
    std::vector<Trade> trades = engine_.get_book(order.symbol)
                                       .process_order(order, next_trade_id_);
    
    if (!trades.empty())
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        for (const auto& trade : trades)
            apply_trade(trade);
    }
    return trades;
}

void OMS::cancel_order(OrderId order_id)
{
    OrderState state;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        auto it = order_states_.find(order_id);
        if (it != order_states_.end())
        {
            state = it->second;
            found = true;
        }
    }
    if (!found) return;

    // Bug fix: is_terminal() now includes Rejected — won't try to cancel
    // a rejected (never-booked) order or double-cancel a filled one
    if (state.is_terminal()) return;

    engine_.get_book(state.symbol).cancel_order(order_id, state.price, state.side);
    
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_[order_id].order_status = OrderStatus::Cancelled;
    }
}

std::vector<Trade> OMS::modify_order(OrderId order_id, Price new_price, Quantity new_qty,
                                     const RiskContext& risk_ctx)
{
    OrderState state;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        auto it = order_states_.find(order_id);
        if (it != order_states_.end())
        {
            state = it->second;
            found = true;
        }
    }
    if (!found) return {};

    // Only live orders can be modified
    if (state.is_terminal()) return {};

    // Step 1: Pull resting (unfilled) quantity from the book
    engine_.get_book(state.symbol).cancel_order(order_id, state.price, state.side);

    // Step 2: Compute how many shares still need to be traded
    Quantity open_qty = new_qty - state.filled_quantity;
    if (open_qty <= 0)
    {
        // New qty <= already filled — treat as a cancel
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_[order_id].order_status = OrderStatus::Cancelled;
        return {};
    }

    // Step 3: Risk-check the revised order
    Order revised;
    revised.order_id   = order_id;
    revised.price      = new_price;
    revised.symbol     = state.symbol;
    revised.side       = state.side;
    revised.order_type = state.order_type;
    revised.quantity   = open_qty;

    RiskResult result = risk_engine_.check_order(risk_ctx, revised);
    if (result.check_type == RiskCheckType::Rejected)
    {
        // Risk rejects the revision — leave order cancelled (don't re-book it)
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_[order_id].order_status = OrderStatus::Cancelled;
        return {};
    }

    // Step 4: Commit state update and re-insert into book (loses time priority)
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        order_states_[order_id].price             = new_price;
        order_states_[order_id].original_quantity = new_qty;
        order_states_[order_id].order_status      = OrderStatus::New;
    }

    std::vector<Trade> trades = engine_.get_book(state.symbol)
                                       .process_order(revised, next_trade_id_);
    
    if (!trades.empty())
    {
        std::lock_guard<std::mutex> lock(oms_mutex_);
        for (const auto& trade : trades)
            apply_trade(trade);
    }
    return trades;
}

// ── Accessors ─────────────────────────────────────────────────────────────────

OrderState OMS::get_order_state(OrderId order_id) const
{
    std::lock_guard<std::mutex> lock(oms_mutex_);
    return order_states_.at(order_id);
}

std::vector<Trade> OMS::get_trades() const
{
    std::lock_guard<std::mutex> lock(oms_mutex_);
    return trades_;
}

std::vector<Execution> OMS::get_executions() const
{
    std::lock_guard<std::mutex> lock(oms_mutex_);
    return executions_;
}
