#include "matching/order_book.h"
#include <algorithm>

std::vector<Execution> OrderBook::process_order(Order incoming)
{
    std::vector<Execution> executions;

    // ---------- BUY ORDER ----------
    if (incoming.side == Side::Buy)
    {
        while (!asks.empty() && incoming.quantity > 0)
        {
            auto bestAskIt = asks.begin();
            Price bestAskPrice = bestAskIt->first;

            if (bestAskPrice > incoming.price)
            {

                break;
            }

            auto &queue = bestAskIt->second;
            Order &resting = queue.front();

            Quantity traded =
                std::min(incoming.quantity, resting.quantity);

            executions.emplace_back(
                incoming.order_id,
                resting.order_id,
                traded,
                bestAskPrice);

            incoming.quantity -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0)
            {
                queue.pop_front();
                if (queue.empty())
                    asks.erase(bestAskIt);
            }
        }

        if (incoming.quantity > 0)
            bids[incoming.price].push_back(incoming);
    }

    // ---------- SELL ORDER ----------
    else if (incoming.side == Side::Sell)
    {
        while (!bids.empty() && incoming.quantity > 0)
        {
            auto bestBidIt = bids.begin();
            Price bestBidPrice = bestBidIt->first;

            if (bestBidPrice < incoming.price)
                break;

            auto &queue = bestBidIt->second;
            Order &resting = queue.front();

            Quantity traded =
                std::min(incoming.quantity, resting.quantity);

            executions.emplace_back(
                incoming.order_id,
                resting.order_id,
                traded,
                bestBidPrice);

            incoming.quantity -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0)
            {
                queue.pop_front();
                if (queue.empty())
                    bids.erase(bestBidIt);
            }
        }

        if (incoming.quantity > 0)
            asks[incoming.price].push_back(incoming);
    }

    return executions;
}

Quantity OrderBook::cancel_order(OrderId order_id, Price price, Side side)
{
    auto cancel_from_level = [&](auto& levels) -> Quantity {
        auto level_it = levels.find(price);
        if (level_it == levels.end())
            return 0;
        
        auto& queue = level_it->second;
        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            if (it->order_id == order_id)
            {
                Quantity cancelled_qty = it->quantity;
                queue.erase(it);
                if (queue.empty())
                    levels.erase(level_it);
                return cancelled_qty;
            }
        }
        return 0;
    };

    return (side == Side::Buy) ? cancel_from_level(bids) : cancel_from_level(asks);
}
