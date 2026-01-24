#include <iostream>
#include <vector>
#include "core/oms.h"
#include "matching/order_book.h"
#include "common/types.h"
#include "common/enums.h"

void print_order_update(OrderId order_id, const OMS &oms)
{
    const auto &state = oms.get_order_state(order_id);
    std::cout << "Order " << order_id << " Status: ";

    switch (state.order_status)
    {
    case OrderStatus::New:
        std::cout << "New";
        break;
    case OrderStatus::Partial_Filled:
        std::cout << "Partially Filled";
        break;
    case OrderStatus::Filled:
        std::cout << "Filled";
        break;
    case OrderStatus::Cancelled:
        std::cout << "Cancelled";
        break;
    case OrderStatus::Rejected:
        std::cout << "Rejected";
        break;
    }

    std::cout << " | Filled: " << state.filled_quantity
              << "/" << state.original_quantity << "\n";
}

int main()
{
    std::cout << "=== Indian Stock Exchange Trading System ===\n\n";

    OrderBook order_book;
    OMS oms(order_book);

    std::cout << "[Step 1] Submitting Sell Orders (Liquidity)...\n";

    Order sell1{1, 2500.0, "RELIANCE", Side::Sell, OrderType::Limit, 100, 100};
    oms.submit_order(sell1);

    Order sell2{2, 3800.0, "TCS", Side::Sell, OrderType::Limit, 50, 50};
    oms.submit_order(sell2);

    Order sell3{3, 1500.0, "INFY", Side::Sell, OrderType::Limit, 200, 200};
    oms.submit_order(sell3);

    std::cout << "Liquidity added for RELIANCE, TCS, INFY.\n\n";

    std::cout << "[Step 2] Submitting Buy Orders (Execution)...\n";

    std::cout << ">> Buyer wants 10 RELIANCE @ 2500.0\n";
    Order buy1{4, 2500.0, "RELIANCE", Side::Buy, OrderType::Limit, 10, 10};
    oms.submit_order(buy1);
    print_order_update(4, oms);
    print_order_update(1, oms);
    std::cout << "\n";

    std::cout << ">> Buyer wants 50 TCS @ 3800.0\n";
    Order buy2{5, 3800.0, "TCS", Side::Buy, OrderType::Limit, 50, 50};
    oms.submit_order(buy2);
    print_order_update(5, oms);
    print_order_update(2, oms);
    std::cout << "\n";

    std::cout << "[Step 3] Submitting Passive Buy Order...\n";
    std::cout << ">> Buyer wants 20 INFY @ 1490.0 (Below Market)\n";
    Order buy3{6, 1490.0, "INFY", Side::Buy, OrderType::Limit, 20, 20};
    oms.submit_order(buy3);
    print_order_update(6, oms);
    std::cout << "\n";

    std::cout << "[Step 4] Cancelling Order...\n";
    std::cout << ">> Cancelling INFY Buy Order (ID: 6)\n";
    oms.cancel_order(6);
    print_order_update(6, oms);

    std::cout << "\n=== Trading Session Ended ===\n";
    return 0;
}
