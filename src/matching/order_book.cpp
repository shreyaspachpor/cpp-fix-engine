#include "matching/order_book.h"
#include <algorithm>

template <typename BookMap, typename PriceCheck>
void OrderBook::match_orders(Order &incoming, BookMap &match_book,
                             std::vector<Execution> &executions,
                             PriceCheck price_check)
{
    while (!match_book.empty() && incoming.quantity > 0)
    {
        auto best_it = match_book.begin();
        Price best_price = best_it->first;

        if (!price_check(best_price, incoming.price))
            break;

        auto &queue = best_it->second;
        Order &resting = queue.front();

        Quantity traded = std::min(incoming.quantity, resting.quantity);

        executions.emplace_back(
            incoming.order_id,
            resting.order_id,
            traded,
            best_price);

        incoming.quantity -= traded;
        resting.quantity -= traded;

        if (resting.quantity == 0)
        {
            queue.pop_front();
            if (queue.empty())
                match_book.erase(best_it);
        }
    }
}

std::vector<Execution> OrderBook::process_order(Order incoming)
{
    std::vector<Execution> executions;

    if (incoming.side == Side::Buy)
    {
        match_orders(incoming, asks, executions,
                     [](Price ask, Price bid)
                     { return ask <= bid; });
        if (incoming.quantity > 0)
            bids[incoming.price].push_back(incoming);
    }
    else if (incoming.side == Side::Sell)
    {
        match_orders(incoming, bids, executions,
                     [](Price bid, Price ask)
                     { return bid >= ask; });
        if (incoming.quantity > 0)
            asks[incoming.price].push_back(incoming);
    }

    return executions;
}

Quantity OrderBook::cancel_order(OrderId order_id, Price price, Side side)
{
    auto cancel_from_level = [&](auto &levels) -> Quantity
    {
        auto level_it = levels.find(price);
        if (level_it == levels.end())
            return 0;

        auto &queue = level_it->second;
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
