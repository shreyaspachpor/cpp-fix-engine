#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <numeric>
#include <cmath>

#include "exchange/exchange.h"

int main()
{
    std::cout << "==================================================\n";
    std::cout << "  CM Matching Engine - High Performance Benchmark \n";
    std::cout << "==================================================\n";

    Exchange exchange;
    exchange.register_symbol("RELIANCE");

    RiskContext risk_ctx;
    risk_ctx.account_id               = 1001;
    risk_ctx.available_margin         = 100000000.0;
    risk_ctx.max_notional_per_order   = 10000000.0;
    risk_ctx.max_notional_per_account = 200000000.0;
    risk_ctx.max_qty_per_order        = 1000000;
    risk_ctx.max_qty_per_account      = 10000000;
    risk_ctx.var_percentage["RELIANCE"]          = 0.15;
    risk_ctx.elm_percentage["RELIANCE"]          = 0.05;
    risk_ctx.price_reference["RELIANCE"]         = 100.0;
    risk_ctx.upper_circuit_limit["RELIANCE"]     = 5000.0;
    risk_ctx.lower_circuit_limit["RELIANCE"]     = 1.0;
    risk_ctx.max_notional_per_symbol["RELIANCE"] = 100000000.0;
    risk_ctx.max_qty_per_symbol["RELIANCE"]      = 5000000;

    constexpr int num_orders = 1000000;
    std::vector<Order> orders;
    orders.reserve(num_orders);

    // Seed for reproducibility
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> price_dist(99.0, 101.0);
    std::uniform_int_distribution<int> qty_dist(10, 500);
    std::uniform_int_distribution<int> side_dist(0, 1);

    // Pre-generate orders to exclude generation overhead from matching latency
    for (int i = 0; i < num_orders; ++i)
    {
        Order order;
        order.order_id   = i + 1;
        order.symbol     = "RELIANCE";
        order.price      = std::round(price_dist(rng) * 100.0) / 100.0; // round to 2 decimals
        order.quantity   = qty_dist(rng);
        order.side       = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
        order.order_type = OrderType::Limit;
        orders.push_back(order);
    }

    std::cout << "Generated " << num_orders << " orders. Starting benchmark...\n";

    std::vector<uint32_t> latencies_ns;
    latencies_ns.reserve(num_orders);

    auto total_start = std::chrono::high_resolution_clock::now();

    for (const auto& order : orders)
    {
        auto start = std::chrono::high_resolution_clock::now();
        exchange.submit_order(order, risk_ctx);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies_ns.push_back(static_cast<uint32_t>(duration));
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

    // Compute stats
    double total_time_sec = total_duration_ms / 1000.0;
    double throughput = num_orders / total_time_sec;

    // Sort latencies to compute percentiles
    std::sort(latencies_ns.begin(), latencies_ns.end());

    uint64_t sum = std::accumulate(latencies_ns.begin(), latencies_ns.end(), 0ULL);
    double mean = static_cast<double>(sum) / num_orders;
    
    uint32_t p50  = latencies_ns[static_cast<size_t>(num_orders * 0.50)];
    uint32_t p90  = latencies_ns[static_cast<size_t>(num_orders * 0.90)];
    uint32_t p95  = latencies_ns[static_cast<size_t>(num_orders * 0.95)];
    uint32_t p99  = latencies_ns[static_cast<size_t>(num_orders * 0.99)];
    uint32_t p999 = latencies_ns[static_cast<size_t>(num_orders * 0.999)];

    std::cout << "\n---------------- Benchmark Results ----------------\n";
    std::cout << "  Total Orders Processed : " << num_orders << "\n";
    std::cout << "  Total Execution Time   : " << total_time_sec << " seconds\n";
    std::cout << "  Throughput             : " << std::fixed << throughput << " orders/sec (" 
              << (throughput / 1000000.0) << " M orders/sec)\n";
    std::cout << "\n  Latency Distribution:\n";
    std::cout << "    Mean Latency         : " << std::fixed << mean << " ns\n";
    std::cout << "    Median (p50)         : " << p50 << " ns\n";
    std::cout << "    90th Percentile (p90): " << p90 << " ns\n";
    std::cout << "    95th Percentile (p95): " << p95 << " ns\n";
    std::cout << "    99th Percentile (p99): " << p99 << " ns\n";
    std::cout << "    99.9th Percentile    : " << p999 << " ns\n";
    std::cout << "---------------------------------------------------\n";

    return 0;
}
