#pragma once

#include "common/types.h"
#include "core/matching_engine.h"
#include "core/oms.h"
#include "core/order.h"
#include "core/order_state.h"
#include "core/trade.h"
#include "risk/risk_engine.h"
#include "risk/risk_context.h"

class Exchange {
public:
    Exchange();
    ~Exchange();

    void register_symbol(const Symbol& symbol);
    std::vector<Trade> submit_order(const Order& order, const RiskContext& risk_ctx);
    void cancel_order(OrderId order_id);
    std::vector<Trade> modify_order(OrderId order_id, Price new_price, Quantity new_qty,
                      const RiskContext& risk_ctx);

    OrderState         get_order_state(OrderId id) const;
    std::vector<Trade> get_trades()                const;

private:
    // Declaration order matters: oms_ takes references to the first two.
    CMRiskEngine   risk_engine_;
    MatchingEngine matching_engine_;
    OMS            oms_;
};
