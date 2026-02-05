#pragma once

#include "common/enums.h"
#include "common/types.h"
#include "core/order.h"
#include "core/execution.h"

#include <map>
#include <deque>
#include <vector>
#include <functional>

class OrderBook {
public:
    std::vector<Execution> process_order(Order incoming);
    Quantity cancel_order(OrderId order_id, Price price, Side side);

private:
    std::map<Price, std::deque<Order>, std::greater<Price>> bids;
    std::map<Price, std::deque<Order>> asks;
};
