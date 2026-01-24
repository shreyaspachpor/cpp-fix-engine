#pragma once

#include <unordered_map>

#include "core/order.h"
#include "core/order_state.h"
#include "core/execution.h"

class OrderBook;

class OMS
{
public:
    // OMS does not own the OrderBook, it coordinates with it
    explicit OMS(OrderBook& order_book);

    void submit_order(const Order& order);
    void cancel_order(OrderId order_id);
    
    const OrderState& get_order_state(OrderId order_id) const;

private:
    void apply_execution(const Execution& execution);
    OrderBook& order_book_;   
    std::unordered_map<OrderId, OrderState> order_states_;
};

