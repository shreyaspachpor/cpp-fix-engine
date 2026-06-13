#pragma once

#include "common/enums.h"
#include "common/types.h"
#include "core/order.h"
#include "core/trade.h"

#include <map>
#include <deque>
#include <vector>
#include <functional>

#include <atomic>

class OrderBook
{
public:
    // next_trade_id is owned by OMS (global counter) — passed in by reference
    // so trade IDs are unique across all symbols, not just per-book.
    std::vector<Trade> process_order(Order incoming, std::atomic<TradeId>& next_trade_id);
    Quantity cancel_order(OrderId order_id, Price price, Side side);

private:
    template <typename BookMap, typename PriceCheck>
    void match_orders(Order& incoming, BookMap& match_book,
                      std::vector<Trade>& trades, PriceCheck price_check,
                      std::atomic<TradeId>& next_trade_id);

    std::map<Price, std::deque<Order>, std::greater<Price>> bids;  // best bid first (highest)
    std::map<Price, std::deque<Order>>                      asks;  // best ask first (lowest)
};
