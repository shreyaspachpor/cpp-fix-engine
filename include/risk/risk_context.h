#pragma once

#include <string>
#include <unordered_map>

#include "common/types.h"
#include "common/enums.h"
#include "core/order.h"
#include "core/order_state.h"
#include <stdexcept>

struct RiskContext
{
    AccountId account_id      = 0;
    double    available_margin = 0.0;

    // Symbol-wise margin & circuit limits
    std::unordered_map<Symbol, double> var_percentage;
    std::unordered_map<Symbol, double> elm_percentage;
    std::unordered_map<Symbol, double> price_reference;
    std::unordered_map<Symbol, double> upper_circuit_limit;
    std::unordered_map<Symbol, double> lower_circuit_limit;

    // Notional limits
    double max_notional_per_order   = 0.0;
    double max_notional_per_account = 0.0;
    std::unordered_map<Symbol, double> max_notional_per_symbol;

    // Quantity limits
    Quantity max_qty_per_order   = 0;
    Quantity max_qty_per_account = 0;
    std::unordered_map<Symbol, Quantity> max_qty_per_symbol;

    // Loads a RiskContext from a JSON config file.
    // Throws std::runtime_error if the file cannot be opened or parsed.
    static RiskContext load_from_file(const std::string& path);
};
