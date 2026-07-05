//
// Created by Curtis chang on 5/7/26.
//

#include "../StockTicker/StockTicker.h"

#include <cassert>
#include <cstdint>
#include <optional>

void test_known_tickers_parse()
{
    assert(parseStockTicker("AAPL") == StockTicker::AAPL);
    assert(parseStockTicker("VOO") == StockTicker::VOO);
    assert(parseStockTicker("BTCUSDT") == StockTicker::BTCUSDT);
    assert(parseStockTicker("XBTUSD") == StockTicker::XBTUSD);
    assert(parseStockTicker("BTC-PERPETUAL") == StockTicker::BTC_PERPETUAL);
}

void test_unknown_ticker_is_rejected()
{
    std::optional<StockTicker> ticker = parseStockTicker("UNKNOWN_SYMBOL");
    assert(!ticker);
}

void test_ticker_converts_to_string()
{
    assert(toString(StockTicker::AAPL) == "AAPL");
    assert(toString(StockTicker::VOO) == "VOO");
    assert(toString(StockTicker::BTCUSDT) == "BTCUSDT");
    assert(toString(StockTicker::XBTUSD) == "XBTUSD");
    assert(toString(StockTicker::BTC_PERPETUAL) == "BTC-PERPETUAL");
    assert(toString(StockTicker::UNKNOWN) == "UNKNOWN");
}

void test_symbol_id_is_stable()
{
    assert(toSymbolId(StockTicker::AAPL) == static_cast<std::uint16_t>(StockTicker::AAPL));
    assert(toSymbolId(StockTicker::VOO) == static_cast<std::uint16_t>(StockTicker::VOO));
    assert(toSymbolId(StockTicker::BTCUSDT) == static_cast<std::uint16_t>(StockTicker::BTCUSDT));
}

int main()
{
    test_known_tickers_parse();
    test_unknown_ticker_is_rejected();
    test_ticker_converts_to_string();
    test_symbol_id_is_stable();

    return 0;
}
