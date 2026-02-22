#pragma once

enum class Side{
    Buy,
    Sell
};

enum class OrderType{
    Limit
};

enum class OrderStatus{
    New,
    Filled,
    Partial_Filled,
    Cancelled,
    Rejected,
};

enum class RiskCheckType{
    Approved,
    Rejected,
    
};

enum class RejectionReason{
    None,
    Insufficient_Margin,
    Circuit_Breaker,
    Max_Notional_Per_Order,
    Max_Notional_Per_Symbol,
    Max_Notional_Per_Account,
    Max_Qty_Per_Order,
    Max_Qty_Per_Symbol,
    Max_Qty_Per_Account,
    Invalid_Order,
};
