#pragma once
#include <string>
#include "common/types.h"
#include "common/enums.h"
// Order (intent)

struct Order{
    OrderId order_id;
    Price price;
    Symbol symbol;
    Side side;
    OrderType order_type;
    Quantity quantity;
    LeavesQty quantity_remaining;


};
