#pragma once

#include "common/enums.h"
#include "common/types.h"
#include "core/order.h"
#include "core/execution.h"
#include <map>
#include <queue>
#include <functional>
#include <vector>

class OrderBook{
public:

    std::vector<Execution> process_order(Order);

private:

    std::map<Price,std::queue<Order>,std::greater<Price>> bids;
    std::map<Price,std::queue<Order>> asks;

};

