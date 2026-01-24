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

