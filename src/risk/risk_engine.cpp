#include "risk/risk_engine.h"


static RiskResult approve()
{
    return RiskResult{ RiskCheckType::Approved, RejectionReason::None };
}

static RiskResult reject(RejectionReason reason)
{
    return RiskResult{ RiskCheckType::Rejected, reason };
}


RiskResult CMRiskEngine::check_order(const RiskContext& risk_ctx, const Order& order) const
{
    RiskResult result = approve();

    if ((result = check_circuit_limits(risk_ctx, order)).check_type    == RiskCheckType::Rejected) return result;
    if ((result = check_margin(risk_ctx, order)).check_type            == RiskCheckType::Rejected) return result;
    if ((result = check_notional_per_order(risk_ctx, order)).check_type  == RiskCheckType::Rejected) return result;
    if ((result = check_notional_per_symbol(risk_ctx, order)).check_type == RiskCheckType::Rejected) return result;
    if ((result = check_notional_per_account(risk_ctx, order)).check_type== RiskCheckType::Rejected) return result;
    if ((result = check_qty_per_order(risk_ctx, order)).check_type       == RiskCheckType::Rejected) return result;
    if ((result = check_qty_per_symbol(risk_ctx, order)).check_type      == RiskCheckType::Rejected) return result;
    if ((result = check_qty_per_account(risk_ctx, order)).check_type     == RiskCheckType::Rejected) return result;

    return approve();
}


RiskResult CMRiskEngine::check_circuit_limits(const RiskContext& risk_ctx, const Order& order) const
{
    auto upper_it = risk_ctx.upper_circuit_limit.find(order.symbol);
    auto lower_it = risk_ctx.lower_circuit_limit.find(order.symbol);

    if (upper_it == risk_ctx.upper_circuit_limit.end() ||
        lower_it == risk_ctx.lower_circuit_limit.end())
        return approve();

    if (order.price > upper_it->second || order.price < lower_it->second)
        return reject(RejectionReason::Circuit_Breaker);

    return approve();
}

RiskResult CMRiskEngine::check_margin(const RiskContext& risk_ctx, const Order& order) const
{
    auto var_it = risk_ctx.var_percentage.find(order.symbol);
    auto elm_it = risk_ctx.elm_percentage.find(order.symbol);

    if (var_it == risk_ctx.var_percentage.end() ||
        elm_it == risk_ctx.elm_percentage.end())
        return approve();

    double required_margin = order.price * order.quantity * (var_it->second + elm_it->second);

    if (required_margin > risk_ctx.available_margin)
        return reject(RejectionReason::Insufficient_Margin);

    return approve();
}

RiskResult CMRiskEngine::check_notional_per_order(const RiskContext& risk_ctx, const Order& order) const
{
    if (risk_ctx.max_notional_per_order <= 0.0)
        return approve();

    double notional = order.price * order.quantity;

    if (notional > risk_ctx.max_notional_per_order)
        return reject(RejectionReason::Max_Notional_Per_Order);

    return approve();
}

RiskResult CMRiskEngine::check_notional_per_symbol(const RiskContext& risk_ctx, const Order& order) const
{
    auto it = risk_ctx.max_notional_per_symbol.find(order.symbol);

    if (it == risk_ctx.max_notional_per_symbol.end() || it->second <= 0.0)
        return approve();

    double notional = order.price * order.quantity;

    if (notional > it->second)
        return reject(RejectionReason::Max_Notional_Per_Symbol);

    return approve();
}

RiskResult CMRiskEngine::check_notional_per_account(const RiskContext& risk_ctx, const Order& order) const
{
    if (risk_ctx.max_notional_per_account <= 0.0)
        return approve();

    double notional = order.price * order.quantity;

    if (notional > risk_ctx.max_notional_per_account)
        return reject(RejectionReason::Max_Notional_Per_Account);

    return approve();
}

RiskResult CMRiskEngine::check_qty_per_order(const RiskContext& risk_ctx, const Order& order) const
{
    if (risk_ctx.max_qty_per_order <= 0)
        return approve();

    if (order.quantity > risk_ctx.max_qty_per_order)
        return reject(RejectionReason::Max_Qty_Per_Order);

    return approve();
}

RiskResult CMRiskEngine::check_qty_per_symbol(const RiskContext& risk_ctx, const Order& order) const
{
    auto it = risk_ctx.max_qty_per_symbol.find(order.symbol);

    if (it == risk_ctx.max_qty_per_symbol.end() || it->second <= 0)
        return approve();

    if (order.quantity > it->second)
        return reject(RejectionReason::Max_Qty_Per_Symbol);

    return approve();
}

RiskResult CMRiskEngine::check_qty_per_account(const RiskContext& risk_ctx, const Order& order) const
{
    if (risk_ctx.max_qty_per_account <= 0)
        return approve();

    if (order.quantity > risk_ctx.max_qty_per_account)
        return reject(RejectionReason::Max_Qty_Per_Account);

    return approve();
}
