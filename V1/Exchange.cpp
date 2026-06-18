//
// Created by Curtis chang on 2/6/26.
//

#include "Exchange.h"

#include <iostream>

#include "Trade.h"
#include "timer/SimpleTimer.h"

#include <utility>

ExecutionReport Exchange::addOrder(const Trade& trade)
{
    auto [bookIterator, isSuccessfulInsertion] = exchange.try_emplace(trade.stockTicker);

    SimpleTimer timer;
    std::vector<Fill> fulfilledTrades = bookIterator -> second.addOrder(trade);
    int64_t timeNano = timer.elapsedNano();
    std::cout << "Time passed in Nano: " << timeNano << " Order filled: " << (!fulfilledTrades.empty() ? "yes" : "no") << "\n";

    // use move because the caller will never need this information again
    return ExecutionReport{trade.stockTicker, trade.orderType, std::move(fulfilledTrades)};
}
