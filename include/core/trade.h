#pragma once

#include "common/types.h"

struct Trade {
    TradeId  trade_id;
    OrderId  buy_order_id;
    OrderId  sell_order_id;
    Price    price;
    Quantity quantity;

    Trade(TradeId tid, OrderId buy, OrderId sell, Price p, Quantity qty)
        : trade_id(tid), buy_order_id(buy), sell_order_id(sell), price(p), quantity(qty) {}
};
        