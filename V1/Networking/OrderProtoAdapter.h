#ifndef ORDERBOOK_NETWORKING_ORDERPROTOADAPTER_H
#define ORDERBOOK_NETWORKING_ORDERPROTOADAPTER_H

#include "../Trade.h"
#include "../generated/order.pb.h"

#include <optional>

std::optional<Trade> toTrade(const orderbook::Order& order);
orderbook::ExecutionReport toProto(const ExecutionReport& report);

#endif
