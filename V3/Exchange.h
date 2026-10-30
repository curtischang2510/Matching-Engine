//
// Created by Curtis chang on 2/6/26.
//

#ifndef ORDERBOOK_EXCHANGE_H
#define ORDERBOOK_EXCHANGE_H
#include <cstddef>
#include <type_traits>
#include <unordered_map>

#include "Logger/LoggingConfig.h"
#if ORDERBOOK_ENABLE_LOGGING
#include "Logger/AsyncLogger.h"
#endif
#include "OrderBook.h"
#include "StockTicker/StockTicker.h"

struct StockTickerHash
{
    std::size_t operator()(StockTicker ticker) const noexcept
    {
        using Underlying = std::underlying_type_t<StockTicker>;
        return static_cast<std::size_t>(static_cast<Underlying>(ticker));
    }
};

class Exchange {
public:
#if ORDERBOOK_ENABLE_LOGGING
    explicit Exchange(AsyncLogger& logger);
#else
    Exchange() = default;
#endif

    ExecutionReport addOrder(const Trade& trade);

private:
#if ORDERBOOK_ENABLE_LOGGING
    AsyncLogger& logger_;
#endif
    std::unordered_map<StockTicker, OrderBook, StockTickerHash> exchange;
};



#endif //ORDERBOOK_EXCHANGE_H
