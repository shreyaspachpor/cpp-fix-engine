#pragma once

#include "common/types.h"
#include "common/enums.h"

struct OrderState
{
    OrderId     order_id;
    Symbol      symbol;
    Quantity    original_quantity;
    Quantity    filled_quantity;
    Price       price;
    Side        side;
    OrderType   order_type;      // needed to reconstruct order on modify
    OrderStatus order_status;

    OrderState()
        : order_id(0),
          symbol(""),
          original_quantity(0),
          filled_quantity(0),
          price(0.0),
          side(Side::Buy),
          order_type(OrderType::Limit),
          order_status(OrderStatus::Rejected)
    {}

    OrderState(OrderId id, Symbol sym, Quantity qty, Price p, Side s, OrderType otype)
        : order_id(id),
          symbol(std::move(sym)),
          original_quantity(qty),
          filled_quantity(0),
          price(p),
          side(s),
          order_type(otype),
          order_status(OrderStatus::New)
    {}

    Quantity remaining_quantity() const
    {
        return original_quantity - filled_quantity;
    }

    // Bug fix: Rejected is also terminal — prevents double-cancel
    bool is_terminal() const
    {
        return order_status == OrderStatus::Filled
            || order_status == OrderStatus::Cancelled
            || order_status == OrderStatus::Rejected;
    }
};
