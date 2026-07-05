//
// Created by Curtis chang on 2/6/26.
//

#include "Exchange.h"

#include "Trade.h"

#include <chrono>
#include <cstdint>
#include <utility>

Exchange::Exchange(AsyncLogger& logger)
    : logger_(logger)
{
}

ExecutionReport Exchange::addOrder(const Trade& trade)
{
    auto [bookIterator, isSuccessfulInsertion] = exchange.try_emplace(trade.stockTicker);

    std::vector<Fill> fulfilledTrades = bookIterator -> second.addOrder(trade);

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    logger_.tryLog(LogEvent{
        .timestampNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
        ),
        .priceTicks = static_cast<std::uint64_t>(trade.price * 100),
        .symbolId = 0,
        .qty = static_cast<std::uint32_t>(trade.qty),
        .latency = 0,
        .type = LogEventType::OrderAdded,
    });

    // use move because the caller will never need this information again
    return ExecutionReport{trade.stockTicker, trade.orderType, std::move(fulfilledTrades)};
}
