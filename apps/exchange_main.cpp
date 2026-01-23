// Simple driver / test
#include "matching/order_book.h"
#include <iostream>

int main()
{
    OrderBook book;

    // Create some test orders
    Order buyOrder1{1, 100.0, "AAPL", Side::Buy, OrderType::Limit, 10, 10};
    Order buyOrder2{2, 99.0, "AAPL", Side::Buy, OrderType::Limit, 5, 5};
    Order sellOrder1{3, 101.0, "AAPL", Side::Sell, OrderType::Limit, 8, 8};
    Order sellOrder2{4, 100.0, "AAPL", Side::Sell, OrderType::Limit, 12, 12};

    // Process orders
    std::cout << "Processing Buy Order 1 @ 100.0 qty 10\n";
    auto exec1 = book.process_order(buyOrder1);
    std::cout << "  Executions: " << exec1.size() << "\n\n";

    std::cout << "Processing Buy Order 2 @ 99.0 qty 5\n";
    auto exec2 = book.process_order(buyOrder2);
    std::cout << "  Executions: " << exec2.size() << "\n\n";

    std::cout << "Processing Sell Order 1 @ 101.0 qty 8\n";
    auto exec3 = book.process_order(sellOrder1);
    std::cout << "  Executions: " << exec3.size() << "\n\n";

    std::cout << "Processing Sell Order 2 @ 100.0 qty 12\n";
    auto exec4 = book.process_order(sellOrder2);
    std::cout << "  Executions: " << exec4.size() << "\n";
    for (const auto &exec : exec4)
    {
        std::cout << "    Order " << exec.order_id << " executed "
                  << exec.quantity << " @ " << exec.price << "\n";
    }

    std::cout << "\nOrder book test completed successfully!\n";
    return 0;
}
