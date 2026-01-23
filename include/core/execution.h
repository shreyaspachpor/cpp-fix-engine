#pragma once

#include "common/types.h"
#include "common/enums.h"

struct Execution
{
    OrderId order_id;
    Quantity quantity;
    Price price;

    Execution(OrderId id, Quantity qty, Price p): order_id(id), quantity(qty), price(p) {}
};
