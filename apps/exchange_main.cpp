#include <iostream>
#include <iomanip>

#include "exchange/exchange.h"

static void print_order(OrderId id, const Exchange& exchange)
{
    const auto& s = exchange.get_order_state(id);
    const char* status = "New";
    if      (s.order_status == OrderStatus::Filled)         status = "Filled";
    else if (s.order_status == OrderStatus::Partial_Filled) status = "PartialFilled";
    else if (s.order_status == OrderStatus::Cancelled)      status = "Cancelled";
    else if (s.order_status == OrderStatus::Rejected)       status = "Rejected";

    std::cout << "  Order #" << id
              << "  " << std::left << std::setw(14) << status
              << "  filled=" << s.filled_quantity << "/" << s.original_quantity << "\n";
}

static void print_trades(const Exchange& exchange, size_t from)
{
    const auto& trades = exchange.get_trades();
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
    // ── Exchange setup ──────────────────────────────────────────────────────
    Exchange exchange;
    exchange.register_symbol("RELIANCE");
    exchange.register_symbol("INFY");

    // ── Risk context (covers both symbols) ──────────────────────────────────
    RiskContext risk_ctx;
    risk_ctx.account_id            = 1001;
    risk_ctx.available_margin      = 500000.0;
    risk_ctx.max_notional_per_order   = 100000.0;
    risk_ctx.max_notional_per_account = 1000000.0;
    risk_ctx.max_qty_per_order        = 5000;
    risk_ctx.max_qty_per_account      = 50000;

    // RELIANCE risk params
    risk_ctx.var_percentage["RELIANCE"]      = 0.15;
    risk_ctx.elm_percentage["RELIANCE"]      = 0.05;
    risk_ctx.price_reference["RELIANCE"]     = 100.00;
    risk_ctx.upper_circuit_limit["RELIANCE"] = 110.00;
    risk_ctx.lower_circuit_limit["RELIANCE"] = 90.00;
    risk_ctx.max_notional_per_symbol["RELIANCE"] = 300000.0;
    risk_ctx.max_qty_per_symbol["RELIANCE"]       = 10000;

    // INFY risk params
    risk_ctx.var_percentage["INFY"]      = 0.12;
    risk_ctx.elm_percentage["INFY"]      = 0.04;
    risk_ctx.price_reference["INFY"]     = 1500.00;
    risk_ctx.upper_circuit_limit["INFY"] = 1650.00;
    risk_ctx.lower_circuit_limit["INFY"] = 1350.00;
    risk_ctx.max_notional_per_symbol["INFY"] = 500000.0;
    risk_ctx.max_qty_per_symbol["INFY"]       = 5000;

    size_t cursor = 0;

    // ═════════════════════════════════════════════════════════════════════════
    // TEST 1: RELIANCE matching (regression — same as Phase 1)
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "=== RELIANCE: passive sell orders ===\n";
    exchange.submit_order({1, 100.00, "RELIANCE", Side::Sell, OrderType::Limit, 100, 100}, risk_ctx);
    exchange.submit_order({2, 101.00, "RELIANCE", Side::Sell, OrderType::Limit,  50,  50}, risk_ctx);
    exchange.submit_order({3, 102.00, "RELIANCE", Side::Sell, OrderType::Limit,  75,  75}, risk_ctx);
    print_order(1, exchange); print_order(2, exchange); print_order(3, exchange);

    std::cout << "\n=== RELIANCE: BUY 50 @ 100.00 (full match) ===\n";
    exchange.submit_order({4, 100.00, "RELIANCE", Side::Buy, OrderType::Limit, 50, 50}, risk_ctx);
    print_order(4, exchange); print_order(1, exchange);
    print_trades(exchange, cursor); cursor = exchange.get_trades().size();

    // ═════════════════════════════════════════════════════════════════════════
    // TEST 2: INFY matching (new symbol, independent book)
    //   Note: INFY @ 1500, max_notional_per_order = 100k, so max qty ≈ 66
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "\n=== INFY: passive sell orders ===\n";
    exchange.submit_order({5, 1500.00, "INFY", Side::Sell, OrderType::Limit, 60, 60}, risk_ctx);
    exchange.submit_order({6, 1510.00, "INFY", Side::Sell, OrderType::Limit, 40, 40}, risk_ctx);
    print_order(5, exchange); print_order(6, exchange);

    std::cout << "\n=== INFY: BUY 50 @ 1500.00 ===\n";
    exchange.submit_order({7, 1500.00, "INFY", Side::Buy, OrderType::Limit, 50, 50}, risk_ctx);
    print_order(7, exchange); print_order(5, exchange);
    print_trades(exchange, cursor); cursor = exchange.get_trades().size();

    // ═════════════════════════════════════════════════════════════════════════
    // TEST 3: Cross-symbol isolation — INFY buy should NOT match RELIANCE sell
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "\n=== Cross-symbol isolation: INFY BUY 30 @ 1600 ===\n";
    exchange.submit_order({8, 1600.00, "INFY", Side::Buy, OrderType::Limit, 30, 30}, risk_ctx);
    print_order(8, exchange);
    // Should match INFY sell @1510, not any RELIANCE order
    print_trades(exchange, cursor); cursor = exchange.get_trades().size();

    // ═════════════════════════════════════════════════════════════════════════
    // TEST 4: Unknown symbol rejection
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "\n=== Unknown symbol: HDFC (should be rejected) ===\n";
    exchange.submit_order({9, 200.00, "HDFC", Side::Buy, OrderType::Limit, 10, 10}, risk_ctx);
    print_order(9, exchange);

    // ═════════════════════════════════════════════════════════════════════════
    // TEST 5: Cancel routes to correct book
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "\n=== Cancel RELIANCE order #1 ===\n";
    std::cout << "  before: "; print_order(1, exchange);
    exchange.cancel_order(1);
    std::cout << "  after:  "; print_order(1, exchange);

    // ═════════════════════════════════════════════════════════════════════════
    // Full trade log
    // ═════════════════════════════════════════════════════════════════════════
    std::cout << "\n=== Full trade log (" << exchange.get_trades().size() << " trades) ===\n";
    print_trades(exchange, 0);

    return 0;
}
