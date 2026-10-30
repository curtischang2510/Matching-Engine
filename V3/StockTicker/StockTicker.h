//
// Created by Curtis chang on 5/7/26.
//

#ifndef ORDERBOOK_STOCKTICKER_H
#define ORDERBOOK_STOCKTICKER_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>


enum class StockTicker : std::uint16_t
{
    AAPL,
    VOO,
    BTCUSDT,
    XBTUSD,
    BTC_PERPETUAL,
    UNKNOWN,
};

constexpr std::uint64_t stockTickerKey(std::string_view text)
{
    std::uint64_t key = static_cast<std::uint64_t>(text.size()) << 56;
    for (std::size_t i = 0; i < text.size() && i < 7; ++i)
    {
        key |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(text[i])) << (i * 8);
    }

    return key;
}

constexpr std::optional<StockTicker> parseStockTicker(std::string_view text)
{
    switch (stockTickerKey(text))
    {
        case stockTickerKey("AAPL"):         return StockTicker::AAPL;
        case stockTickerKey("VOO"):          return StockTicker::VOO;
        case stockTickerKey("BTCUSDT"):       return StockTicker::BTCUSDT;
        case stockTickerKey("XBTUSD"):        return StockTicker::XBTUSD;
        case stockTickerKey("BTC-PERPETUAL"): return StockTicker::BTC_PERPETUAL;
        default:                              return std::nullopt;
    }
}

constexpr std::string_view toString(StockTicker ticker)
{
    switch (ticker)
    {
        case StockTicker::AAPL:
            return "AAPL";
        case StockTicker::VOO:
            return "VOO";
        case StockTicker::BTCUSDT:
            return "BTCUSDT";
        case StockTicker::XBTUSD:
            return "XBTUSD";
        case StockTicker::BTC_PERPETUAL:
            return "BTC-PERPETUAL";
        case StockTicker::UNKNOWN:
            break;
    }

    return "UNKNOWN";
}

constexpr std::uint16_t toSymbolId(StockTicker ticker)
{
    return static_cast<std::uint16_t>(ticker);
}

#endif //ORDERBOOK_STOCKTICKER_H
