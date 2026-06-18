//
// Created by Curtis chang on 1/6/26.
//

#ifndef ORDERBOOK_ORDERBOOK_H
#define ORDERBOOK_ORDERBOOK_H
#include <deque>
#include <map>
#include <cstddef>
#include <string>
#include <vector>

#include "Trade.h"

class OrderBook {
public:
    // can consider creating a constructor to store the stock ticker this orderbk is for
    std::string ticker;
    std::vector<Fill> addOrder(const Trade&);
    std::size_t bidLevelCount() const;
    std::size_t askLevelCount() const;
private:
    std::pair<double, double> l1OrderBook; // (Highest Bid, Lowest Ask)
    // Key: Price
    // Value: deque of qty allows for O(1) insertion from rear and removal from front
    // interestingly deque also supports O(1) random access because in C++ it is implemented using segmented arrays where each block is of fixed size
    std::map<double, std::deque<int64_t>> l2OrderBookBid;
    std::map<double, std::deque<int64_t>> l2OrderBookAsk;

    MatchResult matchIncomingOrder(const Trade&);
    MatchResult matchIncomingBid(const Trade& tradeRequest);
    MatchResult matchIncomingAsk(const Trade& tradeRequest);

};

#endif //ORDERBOOK_ORDERBOOK_H
