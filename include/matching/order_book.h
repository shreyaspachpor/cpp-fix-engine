#pragma once

#include "common/enums.h"
#include "common/types.h"
#include "core/order.h"
#include "core/trade.h"

#include <map>
#include <deque>
#include <vector>
#include <functional>

class OrderBook
{
public:
    std::vector<Trade> process_order(Order incoming);
    Quantity cancel_order(OrderId order_id, Price price, Side side);

private:
    TradeId next_trade_id_{1};
    template <typename BookMap, typename PriceCheck>
    void match_orders(Order &incoming, BookMap &match_book,std::vector<Trade> &trades,PriceCheck price_check);
    std::map<Price, std::deque<Order>, std::greater<Price>> bids;
    std::map<Price, std::deque<Order>> asks;
};
