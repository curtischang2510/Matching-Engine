#include "OrderProtoAdapter.h"

namespace {

std::optional<OrderType> toOrderType(orderbook::Side side)
{
    if (side == orderbook::BID)
    {
        return OrderType::BID;
    }

    if (side == orderbook::ASK)
    {
        return OrderType::ASK;
    }

    return std::nullopt;
}

orderbook::Side toProtoSide(OrderType orderType)
{
    if (orderType == OrderType::BID)
    {
        return orderbook::BID;
    }

    return orderbook::ASK;
}

} // namespace

std::optional<Trade> toTrade(const orderbook::Order& order)
{
    std::optional<OrderType> orderType = toOrderType(order.side());
    if (!orderType || order.stock_ticker().empty() || order.price() <= 0.0 || order.quantity() <= 0)
    {
        return std::nullopt;
    }

    return Trade{
        order.stock_ticker(),
        *orderType,
        order.price(),
        order.quantity()
    };
}

orderbook::ExecutionReport toProto(const ExecutionReport& report)
{
    orderbook::ExecutionReport protoReport;
    protoReport.set_stock_ticker(report.stockTicker);
    protoReport.set_side(toProtoSide(report.orderType));

    for (const Fill& fill : report.fulfilledTrades)
    {
        orderbook::Fill* protoFill = protoReport.add_fills();
        protoFill->set_price(fill.price);
        protoFill->set_quantity(fill.qty);
    }

    return protoReport;
}
