#pragma once

#include <unordered_map>
#include <vector>

#include "core/order.h"
#include "core/order_state.h"
#include "core/execution.h"
#include "core/trade.h"

class OrderBook;

class OMS
{
public:
    explicit OMS(OrderBook& order_book);

    void submit_order(const Order& order);
    void cancel_order(OrderId order_id);
    
    const OrderState& get_order_state(OrderId order_id) const;
    const std::vector<Trade>& get_trades() const;

private:
    void apply_trade(const Trade& trade);
    OrderBook& order_book_;   
    std::unordered_map<OrderId, OrderState> order_states_;
    std::vector<Trade> trades_;
};

