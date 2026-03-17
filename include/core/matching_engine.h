#pragma once

#include <unordered_map>
#include <memory>
#include "common/types.h"

class OrderBook;

class MatchingEngine {
public:
    ~MatchingEngine();

    void register_symbol(const Symbol& symbol);
    OrderBook& get_book(const Symbol& symbol);
    bool has_symbol(const Symbol& symbol) const;

private:
    std::unordered_map<Symbol, std::unique_ptr<OrderBook>> books_;
};
