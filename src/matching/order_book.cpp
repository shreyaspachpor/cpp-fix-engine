#include "matching/order_book.h"
#include <algorithm>

std::vector<Execution> OrderBook::processOrder(Order order) {
    if (order.side == Side::BUY) {
        return matchBuy(order);
    }
    return matchSell(order);
}


std::vector<Execution> OrderBook::matchBuy(Order& buy) {
    std::vector<Execution> executions;

    while (buy.quantity > 0 && !asks.empty()) {
        auto bestAskIt = asks.begin();          // lowest sell price
        Price askPrice = bestAskIt->first;

        // No price overlap → stop matching
        if (askPrice > buy.price) {
            break;
        }

        auto& askQueue = bestAskIt->second;

        while (buy.quantity > 0 && !askQueue.empty()) {
            Order& sell = askQueue.front();

            Quantity tradedQty = std::min(buy.quantity, sell.quantity);

            executions.push_back({
                buy.id,
                tradedQty,
                askPrice,
                ExecType::PARTIAL_FILL
            });

            buy.quantity  -= tradedQty;
            sell.quantity -= tradedQty;

            if (sell.quantity == 0) {
                askQueue.pop_front();   // FIFO preserved
            }
        }

        if (askQueue.empty()) {
            asks.erase(bestAskIt);      // remove empty price level
        }
    }

    // Leftover BUY becomes resting liquidity
    if (buy.quantity > 0) {
        bids[buy.price].push_back(buy);
    }

    return executions;
}

std::vector<Execution> OrderBook::matchSell(Order& sell) {
    std::vector<Execution> executions;

    while (sell.quantity > 0 && !bids.empty()) {
        auto bestBidIt = bids.begin();           // highest buy price
        Price bidPrice = bestBidIt->first;

        // No price overlap → stop matching
        if (bidPrice < sell.price) {
            break;
        }

        auto& bidQueue = bestBidIt->second;

        while (sell.quantity > 0 && !bidQueue.empty()) {
            Order& buy = bidQueue.front();

            Quantity tradedQty = std::min(sell.quantity, buy.quantity);

            executions.push_back({
                sell.id,
                tradedQty,
                bidPrice,
                ExecType::PARTIAL_FILL
            });

            sell.quantity -= tradedQty;
            buy.quantity  -= tradedQty;

            if (buy.quantity == 0) {
                bidQueue.pop_front();
            }
        }

        if (bidQueue.empty()) {
            bids.erase(bestBidIt);
        }
    }

    // Leftover SELL becomes resting liquidity
    if (sell.quantity > 0) {
        asks[sell.price].push_back(sell);
    }

    return executions;
}
