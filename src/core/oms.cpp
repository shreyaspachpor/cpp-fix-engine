
#include "core/oms.h"
#include "core/order_state.h"
#include "matching/order_book.h"

OMS::OMS(OrderBook &order_book) : order_book_(order_book) {}

void OMS::apply_execution(const Execution &execution)
{
    // Update incoming order
    OrderState &order_state = order_states_.at(execution.order_id);
    order_state.filled_quantity += execution.quantity;
    if (order_state.remaining_quantity() == 0)
    {
        order_state.order_status = OrderStatus::Filled;
    }
    else
    {
        order_state.order_status = OrderStatus::Partial_Filled;
    }

    // Update resting order
    OrderState &resting_state = order_states_.at(execution.resting_order_id);
    resting_state.filled_quantity += execution.quantity;
    if (resting_state.remaining_quantity() == 0)
    {
        resting_state.order_status = OrderStatus::Filled;
    }
    else
    {
        resting_state.order_status = OrderStatus::Partial_Filled;
    }
}

void OMS::submit_order(const Order &order)
{
    OrderState order_state(order.order_id, order.quantity, order.price, order.side);
    order_states_.insert({order.order_id, order_state});
    std::vector<Execution> executions = order_book_.process_order(order);
    for (const auto &execution : executions)
    {
        apply_execution(execution);
    }
}

void OMS::cancel_order(OrderId order_id)
{
    OrderState &order_state = order_states_.at(order_id);
    order_state.order_status = OrderStatus::Cancelled;
}

const OrderState &OMS::get_order_state(OrderId order_id) const
{
    return order_states_.at(order_id);
}
