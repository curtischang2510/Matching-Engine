//
// Created by Curtis chang on 3/6/26.
//

#ifndef ORDERBOOK_TRADE_H
#define ORDERBOOK_TRADE_H
#include <cstdint>
#include <string>
#include <vector>

enum class OrderType
{
    BID,
    ASK,
};

struct Trade
{
    std::string stockTicker;
    OrderType orderType;
    double price;
    int64_t qty;
};

struct Fill
{
    double price;
    int64_t qty;
};

struct MatchResult {
    Trade remainingTrade;
    std::vector<Fill> fulfilledTrades;
};

struct ExecutionReport
{
    std::string stockTicker;
    OrderType orderType;
    std::vector<Fill> fulfilledTrades;
};
#endif //ORDERBOOK_TRADE_H
