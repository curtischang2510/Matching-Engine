#ifndef MARKETMAKER_ITCHPARSER_H
#define MARKETMAKER_ITCHPARSER_H
#include <optional>

#include "itch.h"
#include "Networking/ByteReader.h"

inline std::optional<ItchMessage> parseItch(const std::uint8_t* data, std::size_t size)
{
    constexpr std::uint64_t kTimestampMask = 0x0000FFFFFFFFFFFFULL;

    try
    {
        ByteReader r{data, size};
        const char type = static_cast<char>(r.u8());
        const std::uint64_t word = r.u64();
        ItchHeader h{
            static_cast<std::uint16_t>(word >> 48),
            word & kTimestampMask,
        };

        switch (type)
        {
        case 'A':
            {
                AddOrder m{h};
                m.order_id = r.u64();
                m.side     = (r.u8() == 'B') ? Side::BUY : Side::SELL;
                m.shares   = r.u32();
                r.bytes(m.stock.data(), 8);
                m.price    = r.u32();
                return m;
            }
        case 'E':
            {
                OrderExecuted m{h};
                m.order_id        = r.u64();
                m.executed_shares = r.u32();
                m.match_number    = r.u64();
                return m;
            }
        case 'X':
            {
                OrderCancel m{h};
                m.order_id        = r.u64();
                m.canceled_shares = r.u32();
                return m;
            }
        case 'D':
            {
                OrderDelete m{h};
                m.order_id = r.u64();
                return m;
            }
        default:
            return std::nullopt;
        }
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;       // truncated / malformed message
    }
}

#endif //MARKETMAKER_ITCHPARSER_H