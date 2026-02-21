
#include "core/oms.h"
#include "core/order_state.h"
#include "core/trade.h"
#include "matching/order_book.h"

OMS::OMS(OrderBook &order_book) : order_book_(order_book) {}

void OMS::apply_trade(const Trade &trade)
{
    OrderState &buy_state = order_states_.at(trade.buy_order_id);
    buy_state.filled_quantity += trade.quantity;
    if (buy_state.remaining_quantity() == 0)
    {
        buy_state.order_status = OrderStatus::Filled;
    }
    else
    {
        buy_state.order_status = OrderStatus::Partial_Filled;
    }

    OrderState &sell_state = order_states_.at(trade.sell_order_id);
    sell_state.filled_quantity += trade.quantity;
    if (sell_state.remaining_quantity() == 0)
    {
        sell_state.order_status = OrderStatus::Filled;
    }
    else
    {
        sell_state.order_status = OrderStatus::Partial_Filled;
    }

    trades_.push_back(trade);

    Execution buy_exec{trade.trade_id, trade.buy_order_id, Side::Buy, trade.price, trade.quantity};
    Execution sell_exec{trade.trade_id, trade.sell_order_id, Side::Sell, trade.price, trade.quantity};
}

void OMS::submit_order(const Order &order)
{
    OrderState order_state(order.order_id, order.quantity, order.price, order.side);
    order_states_.insert({order.order_id, order_state});
    
    std::vector<Trade> trades = order_book_.process_order(order);
    for (const auto &trade : trades)
    {
        apply_trade(trade);
    }
}

void OMS::cancel_order(OrderId order_id)
{
    OrderState &order_state = order_states_.at(order_id);
    
    if (!order_state.is_terminal())
    {
        order_book_.cancel_order(order_id, order_state.price, order_state.side);
    }
    
    order_state.order_status = OrderStatus::Cancelled;
}

const OrderState &OMS::get_order_state(OrderId order_id) const
{
    return order_states_.at(order_id);
}

const std::vector<Trade> &OMS::get_trades() const
{
    return trades_;
}
