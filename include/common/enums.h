#pragma once

enum class Side{
    Buy,
    Sell
}

enum class OrderType{
    Limit,
    Market
}


enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELED,
    REJECTED
};

enum class ExecType {
    NEW,
    PARTIAL_FILL,
    FILL,
    CANCELED,
    REJECTED
};

