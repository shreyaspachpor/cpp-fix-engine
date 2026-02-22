#pragma once

#include "risk/i_risk_engine.h"

// CM segment risk engine — implements VAR+ELM margin, circuit limits,
// notional limits, and quantity limits per the NSE/BSE CM rulebook.
class CMRiskEngine : public IRiskEngine
{
public:
    RiskResult check_order(const RiskContext& risk_ctx, const Order& order) const override;

private:
    RiskResult check_circuit_limits      (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_margin              (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_notional_per_order  (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_notional_per_symbol (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_notional_per_account(const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_qty_per_order       (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_qty_per_symbol      (const RiskContext& risk_ctx, const Order& order) const;
    RiskResult check_qty_per_account     (const RiskContext& risk_ctx, const Order& order) const;
};
