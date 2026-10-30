//
// Created by Curtis chang on 2/6/26.
//

#include "Exchange.h"

#include "Trade.h"

#include <utility>

ExecutionReport Exchange::addOrder(const Trade& trade)
{
    auto [bookIterator, isSuccessfulInsertion] = exchange.try_emplace(trade.stockTicker);

    std::vector<Fill> fulfilledTrades = bookIterator -> second.addOrder(trade);

    // use move because the caller will never need this information again
    return ExecutionReport{trade.stockTicker, trade.orderType, std::move(fulfilledTrades)};
}
