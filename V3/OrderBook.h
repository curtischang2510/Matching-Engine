//
// Created by Curtis chang on 1/6/26.
//

#ifndef ORDERBOOK_ORDERBOOK_H
#define ORDERBOOK_ORDERBOOK_H
#include <map>
#include <cstddef>
#include <vector>

#include "CircularBuffer/CircularBuffer.h"
#include "Trade.h"

class OrderBook {
public:
    std::vector<Fill> addOrder(const Trade&);
    std::size_t bidLevelCount() const;
    std::size_t askLevelCount() const;
private:
    static constexpr std::size_t InitialPriceLevelCapacity = 1024;
    using PriceLevelQueue = CircularBuffer<int64_t>;

    std::pair<double, double> l1OrderBook; // (Highest Bid, Lowest Ask)
    // Key: Price
    // Value: FIFO quantities at that price, backed by a circular buffer.
    std::map<double, PriceLevelQueue> l2OrderBookBid;
    std::map<double, PriceLevelQueue> l2OrderBookAsk;

    MatchResult matchIncomingOrder(const Trade&);
    MatchResult matchIncomingBid(const Trade& tradeRequest);
    MatchResult matchIncomingAsk(const Trade& tradeRequest);

};

#endif //ORDERBOOK_ORDERBOOK_H
