#include "matching/order_book.h"
#include "exchange/exchange.h"

Exchange::Exchange()
    : oms_(matching_engine_, risk_engine_) {}

Exchange::~Exchange() = default;

void Exchange::register_symbol(const Symbol& symbol)
{
    matching_engine_.register_symbol(symbol);
}

void Exchange::submit_order(const Order& order, const RiskContext& risk_ctx)
{
    oms_.submit_order(order, risk_ctx);
}

void Exchange::cancel_order(OrderId order_id)
{
    oms_.cancel_order(order_id);
}

const OrderState& Exchange::get_order_state(OrderId id) const
{
    return oms_.get_order_state(id);
}

const std::vector<Trade>& Exchange::get_trades() const
{
    return oms_.get_trades();
}
