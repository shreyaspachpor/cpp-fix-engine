#include "core/matching_engine.h"
#include "matching/order_book.h"

MatchingEngine::~MatchingEngine() = default;

void MatchingEngine::register_symbol(const Symbol& symbol)
{
    if (books_.count(symbol) == 0)
    {
        books_[symbol] = std::make_unique<OrderBook>();
    }
}

OrderBook& MatchingEngine::get_book(const Symbol& symbol)
{
    return *books_.at(symbol);
}

bool MatchingEngine::has_symbol(const Symbol& symbol) const
{
    return books_.count(symbol) > 0;
}
