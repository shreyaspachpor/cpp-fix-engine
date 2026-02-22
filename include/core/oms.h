#pragma once

#include <unordered_map>
#include <vector>

#include "core/order.h"
#include "core/order_state.h"
#include "core/execution.h"
#include "core/trade.h"
#include "risk/i_risk_engine.h"
#include "risk/risk_context.h"
#include "risk/risk_result.h"

class OrderBook;

class OMS
{
public:
    // risk_engine is held by reference — OMS does not own it.
    // Caller constructs CMRiskEngine and passes it in.
    explicit OMS(OrderBook& order_book, IRiskEngine& risk_engine);

    // Runs risk check first. If rejected, order_state is marked Rejected
    // and the order never reaches the OrderBook.
    void submit_order(const Order& order, const RiskContext& risk_ctx);
    void cancel_order(OrderId order_id);

    const OrderState& get_order_state(OrderId order_id) const;
    const std::vector<Trade>& get_trades() const;

private:
    void apply_trade(const Trade& trade);

    OrderBook&   order_book_;
    IRiskEngine& risk_engine_;

    std::unordered_map<OrderId, OrderState> order_states_;
    std::vector<Trade> trades_;
};
