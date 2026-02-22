#include "risk/risk_context.h"
#include "json.hpp"

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

RiskContext RiskContext::load_from_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("RiskContext: cannot open config file: " + path);

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("RiskContext: JSON parse error: " + std::string(e.what()));
    }

    RiskContext ctx;

    // ── Account-level fields ──────────────────────────────────────────────────
    const auto& acct          = j.at("account");
    ctx.account_id            = acct.at("account_id").get<AccountId>();
    ctx.available_margin      = acct.at("available_margin").get<double>();
    ctx.max_notional_per_order   = acct.at("max_notional_per_order").get<double>();
    ctx.max_notional_per_account = acct.at("max_notional_per_account").get<double>();
    ctx.max_qty_per_order        = acct.at("max_qty_per_order").get<Quantity>();
    ctx.max_qty_per_account      = acct.at("max_qty_per_account").get<Quantity>();

    // ── Per-symbol fields ─────────────────────────────────────────────────────
    for (const auto& entry : j.at("symbols"))
    {
        const Symbol sym = entry.at("symbol").get<Symbol>();

        ctx.var_percentage[sym]        = entry.at("var_percentage").get<double>();
        ctx.elm_percentage[sym]        = entry.at("elm_percentage").get<double>();
        ctx.price_reference[sym]       = entry.at("price_reference").get<double>();
        ctx.upper_circuit_limit[sym]   = entry.at("upper_circuit_limit").get<double>();
        ctx.lower_circuit_limit[sym]   = entry.at("lower_circuit_limit").get<double>();
        ctx.max_notional_per_symbol[sym] = entry.at("max_notional_per_symbol").get<double>();
        ctx.max_qty_per_symbol[sym]      = entry.at("max_qty_per_symbol").get<Quantity>();
    }

    return ctx;
}