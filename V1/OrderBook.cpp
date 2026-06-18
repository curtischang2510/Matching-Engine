//
// Created by Curtis chang on 1/6/26.
//

#include "OrderBook.h"

#include "Trade.h"

// TODO:
// 1. Return orders (if a there is a bid > ask we want to fulfill/return the order)
// 2. L2 snapshot (snapshot aggregate the quantity at every price)
std::vector<Fill> OrderBook::addOrder(const Trade& trade)
{
    auto& bk = (trade.orderType == OrderType::BID)  ? l2OrderBookBid : l2OrderBookAsk;

    MatchResult matchResult = matchIncomingOrder(trade);
    if (matchResult.remainingTrade.qty > 0)
    {
        auto [iter, isSuccessful] = bk.try_emplace(matchResult.remainingTrade.price);
        iter->second.push_back(matchResult.remainingTrade.qty);
    }
    // Once all operations are complete we check if we need to update the L1 order boot ck (might deprecate)

    return matchResult.fulfilledTrades;
}

std::size_t OrderBook::bidLevelCount() const
{
    return l2OrderBookBid.size();
}

std::size_t OrderBook::askLevelCount() const
{
    return l2OrderBookAsk.size();
}

MatchResult OrderBook::matchIncomingOrder(const Trade& tradeRequest)
{
    if (tradeRequest.orderType == OrderType::BID)
    {
        return matchIncomingBid(tradeRequest);
    }
    return matchIncomingAsk(tradeRequest);
}

MatchResult OrderBook::matchIncomingBid(const Trade& tradeRequest)
{
    std::vector<Fill> fulfilledTrades;
    Trade remainingTrade = tradeRequest;
    auto& bk = l2OrderBookAsk;
    // price < lowest ask (immediately sit on the orderbook)
    if (bk.empty() || tradeRequest.price < bk.begin()->first)
    {
        return MatchResult{remainingTrade, fulfilledTrades};
    }

    while (remainingTrade.qty > 0 && !bk.empty() && bk.begin()->first <= tradeRequest.price) {
        // get the lowest ask
        Fill t;
        double lowestAsk = bk.begin() -> first;
        std::deque<int64_t>& lowestAskDeque = bk.begin() -> second;
        t.price = lowestAsk;
        t.qty = 0;

        while (remainingTrade.qty > 0)
        {
            if (remainingTrade.qty < lowestAskDeque.front())
            {
                lowestAskDeque.front() -= remainingTrade.qty;
                t.qty += remainingTrade.qty;
                remainingTrade.qty = 0;
            } else
            {
                t.qty += lowestAskDeque.front();
                remainingTrade.qty -= lowestAskDeque.front();
                lowestAskDeque.pop_front();
            }

            if (lowestAskDeque.empty())
            {
                bk.erase(bk.begin());
                break;
            }
        }
        fulfilledTrades.push_back(t);
    }

    return MatchResult{remainingTrade, fulfilledTrades};
}

MatchResult OrderBook::matchIncomingAsk(const Trade& tradeRequest)
{
    std::vector<Fill> fulfilledTrades;
    Trade remainingTrade = tradeRequest;
    auto& bk = l2OrderBookBid;
    // price > highest bid (immediately sit on the orderbook)
    if (bk.empty() || tradeRequest.price > std::prev(bk.end())->first)
    {
        return MatchResult{remainingTrade, fulfilledTrades};
    }

    while (remainingTrade.qty > 0 && !bk.empty() && std::prev(bk.end())->first >= tradeRequest.price) {
        // get the lowest ask
        Fill t;
        double highestBid = std::prev(bk.end()) -> first;
        std::deque<int64_t>& highestBidDeque = std::prev(bk.end()) -> second;
        t.price = highestBid;
        t.qty = 0;

        while (remainingTrade.qty > 0)
        {
            if (remainingTrade.qty < highestBidDeque.front())
            {
                highestBidDeque.front() -= remainingTrade.qty;
                t.qty += remainingTrade.qty;
                remainingTrade.qty = 0;
            } else
            {
                t.qty += highestBidDeque.front();
                remainingTrade.qty -= highestBidDeque.front();
                highestBidDeque.pop_front();
            }

            if (highestBidDeque.empty())
            {
                bk.erase(std::prev(bk.end()));
                break;
            }
        }
        fulfilledTrades.push_back(t);
    }

    return MatchResult{remainingTrade, fulfilledTrades};
}
