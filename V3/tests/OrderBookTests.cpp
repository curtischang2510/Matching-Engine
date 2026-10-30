//
// Created by Curtis chang on 7/6/26.
//
#include "../OrderBook.h"

#include <cassert>

void test_non_crossing_orders_do_not_fill()
{
    OrderBook book;
    Trade bid{StockTicker::AAPL, OrderType::BID, 100.0, 10};
    std::vector<Fill> bidFills = book.addOrder(bid);
    assert(bidFills.empty());

    Trade ask{StockTicker::AAPL, OrderType::ASK, 101.0, 5};
    std::vector<Fill> askFills = book.addOrder(ask);
    assert(askFills.empty());
}

void test_crossing_order_fills()
{
    OrderBook book;

    Trade ask{StockTicker::AAPL, OrderType::ASK, 100.0, 5};
    book.addOrder(ask);

    Trade bid{StockTicker::AAPL, OrderType::BID, 101.0, 10};
    std::vector<Fill> fills = book.addOrder(bid);

    assert(fills.size() == 1);
    assert(fills[0].price == 100.0);
    assert(fills[0].qty == 5);
    assert(book.bidLevelCount() == 1);
}

void test_price_level_preserves_fifo_after_growth()
{
    OrderBook book;

    for (int i = 0; i < 1025; ++i)
    {
        Trade ask{StockTicker::AAPL, OrderType::ASK, 100.0, 1};
        book.addOrder(ask);
    }

    Trade partialBid{StockTicker::AAPL, OrderType::BID, 101.0, 1024};
    std::vector<Fill> partialFills = book.addOrder(partialBid);

    assert(partialFills.size() == 1);
    assert(partialFills[0].price == 100.0);
    assert(partialFills[0].qty == 1024);
    assert(book.askLevelCount() == 1);

    Trade finalBid{StockTicker::AAPL, OrderType::BID, 101.0, 1};
    std::vector<Fill> finalFills = book.addOrder(finalBid);

    assert(finalFills.size() == 1);
    assert(finalFills[0].price == 100.0);
    assert(finalFills[0].qty == 1);
    assert(book.askLevelCount() == 0);
}

int main()
{
    test_non_crossing_orders_do_not_fill();
    test_crossing_order_fills();
    test_price_level_preserves_fifo_after_growth();

    return 0;
}
