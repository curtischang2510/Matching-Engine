//
// Created by Curtis chang on 2/6/26.
//

#include "Exchange.h"

#include "Trade.h"
#if ORDERBOOK_ENABLE_LOGGING
#include "timer/TimeCall.h"
#endif

#if ORDERBOOK_ENABLE_LOGGING
#include <chrono>
#include <cstdint>
#endif
#include <utility>

#if ORDERBOOK_ENABLE_LOGGING
Exchange::Exchange(AsyncLogger& logger)
    : logger_(logger)
{
}
#endif

ExecutionReport Exchange::addOrder(const Trade& trade)
{
    auto [bookIterator, isSuccessfulInsertion] = exchange.try_emplace(trade.stockTicker);

#if ORDERBOOK_ENABLE_LOGGING
    std::uint32_t latencyNs = 0;
    std::vector<Fill> fulfilledTrades = timeCall(
        [&]
        {
            return bookIterator->second.addOrder(trade);
        },
        latencyNs
    );

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    logger_.tryLog(LogEvent{
        .timestampNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
        ),
        .priceTicks = static_cast<std::uint64_t>(trade.price * 100),
        .symbolId = toSymbolId(trade.stockTicker),
        .qty = static_cast<std::uint32_t>(trade.qty),
        .latency = latencyNs,
        .type = LogEventType::OrderAdded,
    });
#else
    std::vector<Fill> fulfilledTrades = bookIterator->second.addOrder(trade);
#endif

    // use move because the caller will never need this information again
    return ExecutionReport{trade.stockTicker, trade.orderType, std::move(fulfilledTrades)};
}
