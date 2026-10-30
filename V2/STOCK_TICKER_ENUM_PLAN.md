# Stock Ticker Enum Plan

This note describes how to replace repeated stock ticker strings with a compact enum value while keeping the architecture mostly unchanged.

The immediate goal is:

```text
incoming string ticker -> StockTicker enum -> integer-friendly logging
```

The logger should not carry `std::string` values in the hot path. It should log an integer-like ticker id, and a slower consumer/debug path can convert that id back into text.

## Why Use An Enum

Right now a ticker is represented as:

```cpp
std::string stockTicker;
```

Strings are convenient, but they are not ideal in a low-latency path:

- string comparison is more expensive than integer comparison
- strings may allocate
- strings are variable-sized
- logging strings requires copying or lifetime management
- hash maps over strings spend time hashing bytes

An enum gives you a fixed-size representation:

```cpp
enum class StockTicker : std::uint16_t
{
    AAPL,
    BTCUSDT,
    XBTUSD,
    Unknown
};
```

Then logs can store:

```cpp
std::uint16_t symbolId;
```

or:

```cpp
StockTicker ticker;
```

## Recommended First Step

Create a focused ticker module:

```text
V2/
  StockTicker.h
  StockTicker.cpp
```

Keep this separate from `Trade.h` at first. It will be easier to test independently.

## Suggested API

```cpp
// StockTicker.h

#ifndef ORDERBOOK_STOCK_TICKER_H
#define ORDERBOOK_STOCK_TICKER_H

#include <cstdint>
#include <optional>
#include <string_view>

enum class StockTicker : std::uint16_t
{
    AAPL,
    VOO,
    BTCUSDT,
    XBTUSD,
    Unknown
};

std::optional<StockTicker> parseStockTicker(std::string_view text);
std::string_view toString(StockTicker ticker);
std::uint16_t toSymbolId(StockTicker ticker);

#endif
```

Notes:

- Use `std::string_view` so parsing does not require copying the input string.
- Return `std::optional<StockTicker>` for parsing so invalid input is explicit.
- `Unknown` can be useful for logs, but invalid input should probably still be rejected before matching.

## Simple Implementation

Start with clear `if` statements:

```cpp
// StockTicker.cpp

#include "StockTicker.h"

std::optional<StockTicker> parseStockTicker(std::string_view text)
{
    if (text == "AAPL")
    {
        return StockTicker::AAPL;
    }
    if (text == "VOO")
    {
        return StockTicker::VOO;
    }
    if (text == "BTCUSDT")
    {
        return StockTicker::BTCUSDT;
    }
    if (text == "XBTUSD")
    {
        return StockTicker::XBTUSD;
    }

    return std::nullopt;
}

std::string_view toString(StockTicker ticker)
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
        case StockTicker::Unknown:
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

std::uint16_t toSymbolId(StockTicker ticker)
{
    return static_cast<std::uint16_t>(ticker);
}
```

This is not the most optimized implementation, but it is clean and easy to verify.

## Where It Fits

The clean path is:

```text
protobuf order
    -> parse stock_ticker string once
    -> Trade stores StockTicker
    -> Exchange routes by StockTicker
    -> Logger writes symbol id
    -> log consumer can convert id back to string
```

Do not immediately rewrite every layer. A gradual path is safer:

1. Add `StockTicker` module and tests.
2. Parse protobuf `stock_ticker` into `StockTicker`.
3. Add `StockTicker ticker` to `Trade`.
4. Keep the existing string temporarily if needed.
5. Update logging to use `toSymbolId(trade.ticker)`.
6. Later update `Exchange` to route by enum instead of string.

## Logging Use

Current log event has:

```cpp
std::uint32_t symbolId;
```

That should eventually come from:

```cpp
toSymbolId(trade.ticker)
```

Then `writeEvent` can write the numeric id:

```text
timestamp,event_type,symbol_id,price,qty,latency
```

If the consumer wants text:

```cpp
toString(static_cast<StockTicker>(symbolId))
```

## Future Array Optimization

Once tickers are enum values from `0..N-1`, you can replace maps with arrays or vectors:

```cpp
std::array<OrderBook, TickerCount> books;
```

Then:

```cpp
books[toSymbolId(ticker)].addOrder(trade);
```

This avoids string hashing and map lookup.

Do not do this immediately unless you are ready to commit to a fixed ticker universe.

## Important Design Choice

There are two possible ticker policies:

1. **Closed universe**
   - Only known tickers are accepted.
   - Best for performance.
   - Works well with arrays.

2. **Open universe**
   - New tickers can appear from input data.
   - Requires a dynamic registry or map.
   - More flexible but slower and more complex.

For V2, use a closed universe. You are optimizing and learning; accepting arbitrary symbols can come later.

## Tests To Add

Add a `StockTickerTests.cpp` later:

- [ ] `"AAPL"` parses to `StockTicker::AAPL`
- [ ] `"BTCUSDT"` parses to `StockTicker::BTCUSDT`
- [ ] invalid ticker returns `std::nullopt`
- [ ] `toString(StockTicker::AAPL)` returns `"AAPL"`
- [ ] `toSymbolId(...)` is stable

## What Not To Do Yet

- Do not add a dynamic ticker registry yet.
- Do not store strings in `LogEvent`.
- Do not rewrite the whole `Exchange` routing layer in the same change.
- Do not optimize into arrays before enum parsing is tested.

## Recommended First Commit

The first ticker commit should only add:

- `StockTicker.h`
- `StockTicker.cpp`
- `StockTickerTests.cpp`

After that passes, wire it into protobuf conversion and logging.

