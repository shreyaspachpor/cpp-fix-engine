#include <iostream>
#include <string>

#include <quickfix/FileStore.h>
#include <quickfix/FileLog.h>
#include <quickfix/SocketAcceptor.h>
#include <quickfix/SessionSettings.h>

#include "exchange/exchange.h"
#include "fix/exchange_application.h"

int main(int argc, char* argv[])
{
    // Config file path — can override via command line arg
    std::string cfg_path = "config/fix_session.cfg";
    if (argc > 1) cfg_path = argv[1];

    // ── Exchange setup ────────────────────────────────────────────────────────
    Exchange exchange;
    exchange.register_symbol("RELIANCE");
    exchange.register_symbol("INFY");
    exchange.register_symbol("TCS");

    RiskContext risk_ctx;
    risk_ctx.account_id               = 1001;
    risk_ctx.available_margin         = 5000000.0;
    risk_ctx.max_notional_per_order   = 500000.0;
    risk_ctx.max_notional_per_account = 10000000.0;
    risk_ctx.max_qty_per_order        = 10000;
    risk_ctx.max_qty_per_account      = 100000;

    for (const auto& sym : {"RELIANCE", "TCS"})
    {
        risk_ctx.var_percentage[sym]          = 0.15;
        risk_ctx.elm_percentage[sym]          = 0.05;
        risk_ctx.price_reference[sym]         = 100.0;
        risk_ctx.upper_circuit_limit[sym]     = 5000.0;
        risk_ctx.lower_circuit_limit[sym]     = 1.0;
        risk_ctx.max_notional_per_symbol[sym] = 2000000.0;
        risk_ctx.max_qty_per_symbol[sym]      = 50000;
    }
    risk_ctx.var_percentage["INFY"]          = 0.12;
    risk_ctx.elm_percentage["INFY"]          = 0.04;
    risk_ctx.price_reference["INFY"]         = 1500.0;
    risk_ctx.upper_circuit_limit["INFY"]     = 5000.0;
    risk_ctx.lower_circuit_limit["INFY"]     = 1.0;
    risk_ctx.max_notional_per_symbol["INFY"] = 3000000.0;
    risk_ctx.max_qty_per_symbol["INFY"]      = 20000;

    // ── QuickFIX acceptor setup ───────────────────────────────────────────────
    try
    {
        FIX::SessionSettings    settings(cfg_path);
        ExchangeApplication     application(exchange, risk_ctx);
        FIX::FileStoreFactory   store_factory(settings);
        FIX::FileLogFactory     log_factory(settings);
        FIX::SocketAcceptor     acceptor(application, store_factory, settings, log_factory);

        acceptor.start();

        std::cout << "======================================\n";
        std::cout << "  CM Trading System — FIX Acceptor   \n";
        std::cout << "  Listening on port 5001 (FIX 4.2)   \n";
        std::cout << "  Symbols: RELIANCE  INFY  TCS        \n";
        std::cout << "======================================\n";
        std::cout << "Press Enter to stop.\n";
        std::cin.get();

        acceptor.stop();
        std::cout << "[FIX] Acceptor stopped.\n";
    }
    catch (const FIX::ConfigError& e)
    {
        std::cerr << "[ERROR] FIX config error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
