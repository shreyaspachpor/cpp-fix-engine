#pragma once

#include "risk/risk_context.h"
#include "risk/risk_result.h"
#include "core/order.h"

// Abstract interface for any risk engine implementation.
// To add a new risk model (e.g. SPAN for F&O), inherit from this and
// override check_order — nothing else in the system needs to change.
class IRiskEngine
{
public:
    virtual RiskResult check_order(const RiskContext& risk_ctx, const Order& order) const = 0;
    virtual ~IRiskEngine() = default;
};
