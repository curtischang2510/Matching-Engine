//
// Created by Curtis chang on 2/6/26.
//

#ifndef ORDERBOOK_EXCHANGE_H
#define ORDERBOOK_EXCHANGE_H
#include <string>
#include <unordered_map>

#include "Logger/AsyncLogger.h"
#include "OrderBook.h"

class Exchange {
public:
    explicit Exchange(AsyncLogger& logger);

    ExecutionReport addOrder(const Trade& trade);

private:
    AsyncLogger& logger_;
    std::unordered_map<std::string, OrderBook> exchange;
};



#endif //ORDERBOOK_EXCHANGE_H
