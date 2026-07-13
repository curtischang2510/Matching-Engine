#ifndef MARKETMAKER_MOLDUDPREQUEST_H
#define MARKETMAKER_MOLDUDPREQUEST_H

#include <array>
#include <cstdint>

inline std::array<std::uint8_t, 10> buildRequest(std::uint64_t sequence, std::uint16_t count)
{
    std::array<std::uint8_t, 10> out{};
    for (int i = 0; i < 8; ++i)
    {
        out[i] = static_cast<std::uint8_t>((sequence >> (56 - 8 * i)) & 0xFF);
    }
    out[8] = static_cast<std::uint8_t>((count >> 8) & 0xFF);
    out[9] = static_cast<std::uint8_t>(count & 0xFF);
    return out;
}

#endif //MARKETMAKER_MOLDUDPREQUEST_H
