#include <iostream>
#include <iomanip>

#include "core/oms.h"
#include "matching/order_book.h"

static void print_order(OrderId id, const OMS& oms)
{
    const auto& s = oms.get_order_state(id);
    const char* status = "New";
    if (s.order_status == OrderStatus::Filled)         status = "Filled";
    else if (s.order_status == OrderStatus::Partial_Filled) status = "PartialFilled";
    else if (s.order_status == OrderStatus::Cancelled)  status = "Cancelled";

    std::cout << "  Order #" << id
              << "  " << std::left << std::setw(14) << status
              << "  filled=" << s.filled_quantity << "/" << s.original_quantity << "\n";
}

static void print_trades(const OMS& oms, size_t from)
{
    const auto& trades = oms.get_trades();
    for (size_t i = from; i < trades.size(); ++i)
    {
        const auto& t = trades[i];
        std::cout << "  Trade #" << t.trade_id
                  << "  buy=" << t.buy_order_id
                  << "  sell=" << t.sell_order_id
                  << "  price=" << std::fixed << std::setprecision(2) << t.price
                  << "  qty=" << t.quantity << "\n";
    }
}

int main()
{
    OrderBook book;
    OMS oms(book);
    size_t cursor = 0;

    // --- Setup: passive sell-side liquidity
    std::cout << "=== Setup: passive sell orders ===\n";
    oms.submit_order({1, 100.00, "RELIANCE", Side::Sell, OrderType::Limit, 100, 100});
    oms.submit_order({2, 101.00, "RELIANCE", Side::Sell, OrderType::Limit,  50,  50});
    oms.submit_order({3, 102.00, "RELIANCE", Side::Sell, OrderType::Limit,  75,  75});
    print_order(1, oms); print_order(2, oms); print_order(3, oms);

    // --- Full match
    std::cout << "\n=== Full match: BUY 50 @ 100.00 ===\n";
    oms.submit_order({4, 100.00, "RELIANCE", Side::Buy, OrderType::Limit, 50, 50});
    print_order(4, oms); print_order(1, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Partial fill
    std::cout << "\n=== Partial fill: BUY 75 @ 100.00 ===\n";
    oms.submit_order({5, 100.00, "RELIANCE", Side::Buy, OrderType::Limit, 75, 75});
    print_order(5, oms); print_order(1, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Price improvement
    std::cout << "\n=== Price improvement: BUY 60 @ 101.50 ===\n";
    oms.submit_order({6, 101.50, "RELIANCE", Side::Buy, OrderType::Limit, 60, 60});
    print_order(6, oms); print_order(2, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Passive buy, no match
    std::cout << "\n=== Passive: BUY 30 @ 98.00 (no match) ===\n";
    oms.submit_order({7, 98.00, "RELIANCE", Side::Buy, OrderType::Limit, 30, 30});
    print_order(7, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Sell aggressor hits resting buy
    std::cout << "\n=== Sell aggressor: SELL 20 @ 98.00 ===\n";
    oms.submit_order({8, 98.00, "RELIANCE", Side::Sell, OrderType::Limit, 20, 20});
    print_order(8, oms); print_order(7, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Cancel
    std::cout << "\n=== Cancel order #7 ===\n";
    std::cout << "  before: "; print_order(7, oms);
    oms.cancel_order(7);
    std::cout << "  after:  "; print_order(7, oms);

    // --- Book sweep
    std::cout << "\n=== Sweep: BUY 100 @ 105.00 ===\n";
    oms.submit_order({9, 105.00, "RELIANCE", Side::Buy, OrderType::Limit, 100, 100});
    print_order(9, oms); print_order(2, oms); print_order(3, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Full trade log
    std::cout << "\n=== Full trade log (" << oms.get_trades().size() << " trades) ===\n";
    print_trades(oms, 0);

    return 0;
}
