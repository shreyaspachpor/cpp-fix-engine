#pragma once

#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

#include "core/order.h"
#include "core/order_state.h"
#include "core/execution.h"
#include "core/trade.h"
#include "risk/i_risk_engine.h"
#include "risk/risk_context.h"
#include "risk/risk_result.h"

class MatchingEngine;

class OMS
{
public:
    // risk_engine is held by reference — OMS does not own it.
    explicit OMS(MatchingEngine& engine, IRiskEngine& risk_engine);

    // Submit a new order (risk-checked before reaching the book).
    std::vector<Trade> submit_order(const Order& order, const RiskContext& risk_ctx);

    // Cancel a live order. No-op if already terminal (Filled/Cancelled/Rejected).
    void cancel_order(OrderId order_id);

    // Cancel/Replace: pull resting qty, run risk on revised order, re-insert.
    // Loses time priority. If risk rejects the revision, order is cancelled.
    std::vector<Trade> modify_order(OrderId order_id, Price new_price, Quantity new_qty,
                                    const RiskContext& risk_ctx);

    OrderState                 get_order_state(OrderId order_id) const;
    std::vector<Trade>         get_trades()     const;
    std::vector<Execution>     get_executions() const;

private:
    void apply_trade(const Trade& trade);

    MatchingEngine& engine_;
    IRiskEngine&    risk_engine_;
    std::atomic<TradeId> next_trade_id_{1};   // global, unique across all symbols

    mutable std::mutex oms_mutex_;
    std::unordered_map<OrderId, OrderState> order_states_;
    std::vector<Trade>     trades_;
    std::vector<Execution> executions_;  // one per side per trade (buy + sell)
};
