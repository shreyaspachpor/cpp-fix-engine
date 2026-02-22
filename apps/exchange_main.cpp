#include <iostream>
#include <iomanip>

#include "core/oms.h"
#include "matching/order_book.h"
#include "risk/risk_engine.h"
#include "risk/risk_context.h"

static void print_order(OrderId id, const OMS& oms)
{
    const auto& s = oms.get_order_state(id);
    const char* status = "New";
    if      (s.order_status == OrderStatus::Filled)         status = "Filled";
    else if (s.order_status == OrderStatus::Partial_Filled) status = "PartialFilled";
    else if (s.order_status == OrderStatus::Cancelled)      status = "Cancelled";
    else if (s.order_status == OrderStatus::Rejected)       status = "Rejected";

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
                  << "  buy="  << t.buy_order_id
                  << "  sell=" << t.sell_order_id
                  << "  price=" << std::fixed << std::setprecision(2) << t.price
                  << "  qty="  << t.quantity << "\n";
    }
}

int main()
{
    // ── Risk setup ────────────────────────────────────────────────────────────
    CMRiskEngine risk_engine;

    // Manually constructed context matching the test prices (100–105 range)
    RiskContext risk_ctx;
    risk_ctx.account_id            = 1001;
    risk_ctx.available_margin      = 500000.0;
    risk_ctx.max_notional_per_order   = 100000.0;
    risk_ctx.max_notional_per_account = 1000000.0;
    risk_ctx.max_qty_per_order        = 5000;
    risk_ctx.max_qty_per_account      = 50000;

    risk_ctx.var_percentage["RELIANCE"]      = 0.15;
    risk_ctx.elm_percentage["RELIANCE"]      = 0.05;
    risk_ctx.price_reference["RELIANCE"]     = 100.00;
    risk_ctx.upper_circuit_limit["RELIANCE"] = 110.00;
    risk_ctx.lower_circuit_limit["RELIANCE"] = 90.00;
    risk_ctx.max_notional_per_symbol["RELIANCE"] = 300000.0;
    risk_ctx.max_qty_per_symbol["RELIANCE"]       = 10000;

    // ── OMS + OrderBook ───────────────────────────────────────────────────────
    OrderBook book;
    OMS oms(book, risk_engine);
    size_t cursor = 0;

    // --- Setup: passive sell-side liquidity
    std::cout << "=== Setup: passive sell orders ===\n";
    oms.submit_order({1, 100.00, "RELIANCE", Side::Sell, OrderType::Limit, 100, 100}, risk_ctx);
    oms.submit_order({2, 101.00, "RELIANCE", Side::Sell, OrderType::Limit,  50,  50}, risk_ctx);
    oms.submit_order({3, 102.00, "RELIANCE", Side::Sell, OrderType::Limit,  75,  75}, risk_ctx);
    print_order(1, oms); print_order(2, oms); print_order(3, oms);

    // --- Full match
    std::cout << "\n=== Full match: BUY 50 @ 100.00 ===\n";
    oms.submit_order({4, 100.00, "RELIANCE", Side::Buy, OrderType::Limit, 50, 50}, risk_ctx);
    print_order(4, oms); print_order(1, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Partial fill
    std::cout << "\n=== Partial fill: BUY 75 @ 100.00 ===\n";
    oms.submit_order({5, 100.00, "RELIANCE", Side::Buy, OrderType::Limit, 75, 75}, risk_ctx);
    print_order(5, oms); print_order(1, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Price improvement
    std::cout << "\n=== Price improvement: BUY 60 @ 101.50 ===\n";
    oms.submit_order({6, 101.50, "RELIANCE", Side::Buy, OrderType::Limit, 60, 60}, risk_ctx);
    print_order(6, oms); print_order(2, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Passive buy, no match
    std::cout << "\n=== Passive: BUY 30 @ 98.00 (no match) ===\n";
    oms.submit_order({7, 98.00, "RELIANCE", Side::Buy, OrderType::Limit, 30, 30}, risk_ctx);
    print_order(7, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Sell aggressor hits resting buy
    std::cout << "\n=== Sell aggressor: SELL 20 @ 98.00 ===\n";
    oms.submit_order({8, 98.00, "RELIANCE", Side::Sell, OrderType::Limit, 20, 20}, risk_ctx);
    print_order(8, oms); print_order(7, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Cancel
    std::cout << "\n=== Cancel order #7 ===\n";
    std::cout << "  before: "; print_order(7, oms);
    oms.cancel_order(7);
    std::cout << "  after:  "; print_order(7, oms);

    // --- Book sweep
    std::cout << "\n=== Sweep: BUY 100 @ 105.00 ===\n";
    oms.submit_order({9, 105.00, "RELIANCE", Side::Buy, OrderType::Limit, 100, 100}, risk_ctx);
    print_order(9, oms); print_order(2, oms); print_order(3, oms);
    print_trades(oms, cursor); cursor = oms.get_trades().size();

    // --- Risk rejection demo: price above upper circuit (110.00)
    std::cout << "\n=== Risk rejection: BUY @ 115.00 (above circuit) ===\n";
    oms.submit_order({10, 115.00, "RELIANCE", Side::Buy, OrderType::Limit, 10, 10}, risk_ctx);
    print_order(10, oms);

    // --- Full trade log
    std::cout << "\n=== Full trade log (" << oms.get_trades().size() << " trades) ===\n";
    print_trades(oms, 0);

    return 0;
}
