#pragma once

#include "common/types.h"
#include "common/enums.h"

struct Execution
{
    OrderId order_id;         // Incoming order ID
    OrderId resting_order_id; // Resting order ID that matched
    Quantity quantity;
    Price price;

    Execution(OrderId id, OrderId resting_id, Quantity qty, Price p) : order_id(id), resting_order_id(resting_id), quantity(qty), price(p) {}
};
