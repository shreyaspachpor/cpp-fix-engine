#pragma once

#include "common/types.h"
#include "common/enums.h"

struct Execution {
    TradeId  trade_id;
    OrderId  order_id;
    Side     side;
    Price    price;
    Quantity quantity;
};
