#pragma once

#include "common/types.h"
#include "common/enums.h"

struct OrderState
{
    OrderId     order_id;
    Quantity    original_quantity;
    Quantity    filled_quantity;
    OrderStatus order_status;

    OrderState(OrderId id, Quantity qty)
        : order_id(id),
          original_quantity(qty),
          filled_quantity(0),
          order_status(OrderStatus::New)
    {}

    
    Quantity remaining_quantity() const
    {
        return original_quantity - filled_quantity;
    }

    bool is_terminal() const
    {
        return order_status == OrderStatus::Filled ||
               order_status == OrderStatus::Cancelled;
    }
};
