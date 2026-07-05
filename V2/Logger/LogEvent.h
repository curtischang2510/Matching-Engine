//
// Created by Curtis chang on 4/7/26.
//

#ifndef ORDERBOOK_LOGEVENT_H
#define ORDERBOOK_LOGEVENT_H
#include <cstdint>

enum class LogEventType : std::uint8_t
{
    ProtoParsed,
    OrderAdded,
    OrderMatch,
};

struct LogEvent
{
    std::uint64_t timestampNs;
    std::uint64_t priceTicks;
    std::uint32_t symbolId;
    std::uint32_t qty;
    std::uint32_t latency;
    LogEventType type;
    // 3 bytes padding
};

#endif //ORDERBOOK_LOGEVENT_H
