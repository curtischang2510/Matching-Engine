//
// Created by Curtis chang on 7/6/26.
//
#include "../OrderBook.h"

#include <cassert>

void test_non_crossing_orders_do_not_fill()
{
    OrderBook book;
    Trade bid{"AAPL", OrderType::BID, 100.0, 10};
    std::vector<Fill> bidFills = book.addOrder(bid);
    assert(bidFills.empty());

    Trade ask{"AAPL", OrderType::ASK, 101.0, 5};
    std::vector<Fill> askFills = book.addOrder(ask);
    assert(askFills.empty());
}

void test_crossing_order_fills()
{
    OrderBook book;

    Trade ask{"AAPL", OrderType::ASK, 100.0, 5};
    book.addOrder(ask);

    Trade bid{"AAPL", OrderType::BID, 101.0, 10};
    std::vector<Fill> fills = book.addOrder(bid);

    assert(fills.size() == 1);
    assert(fills[0].price == 100.0);
    assert(fills[0].qty == 5);
    assert(book.bidLevelCount() == 1);
}

int main()
{
    test_non_crossing_orders_do_not_fill();
    test_crossing_order_fills();

    return 0;
}
