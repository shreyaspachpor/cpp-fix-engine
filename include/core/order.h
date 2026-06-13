#pragma once
#include <string>
#include "common/types.h"
#include "common/enums.h"

// Order (intent) — immutable description of what the client wants to trade.
// Quantity tracks remaining shares during matching (decremented in-place by OrderBook).
struct Order
{
    OrderId   order_id;
    Price     price;
    Symbol    symbol;
    Side      side;
    OrderType order_type;
    Quantity  quantity;      // shares remaining (starts == original qty, decremented by book)
};
