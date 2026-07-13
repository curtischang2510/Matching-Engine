//
// Created by Curtis chang on 12/7/26.
//

#ifndef MARKETMAKER_ITCH_H
#define MARKETMAKER_ITCH_H
#include <array>
#include <cstdint>
#include <variant>

enum class Side
{
    BUY,
    SELL
};

struct ItchHeader
{
    std::uint16_t stock_locate;
    std::uint64_t timestamp_ns;
};

struct AddOrder
{
    ItchHeader header;
    std::uint64_t order_id;
    Side side;
    std::uint32_t shares;
    std::array<char, 8> stock;
    std::uint32_t price;
};

struct OrderExecuted
{
    ItchHeader header;
    std::uint64_t order_id;
    std::uint32_t executed_shares;
    std::uint64_t match_number; // not used
};

struct OrderCancel {
    ItchHeader header;
    std::uint64_t order_id;
    std::uint32_t canceled_shares;
};

struct OrderDelete {
    ItchHeader header;
    std::uint64_t order_id;
};

using ItchMessage = std::variant<AddOrder, OrderExecuted, OrderCancel, OrderDelete>;

#endif //MARKETMAKER_ITCH_H
