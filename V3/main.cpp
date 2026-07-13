#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <variant>

#include "ITCH/ItchParser.h"
#include "Networking/ByteReader.h"
#include "Networking/MoldUdpRequest.h"
#include "Networking/MulticastReceiver.h"
#include "OrderBook/LocalOrderBook.h"

namespace
{
constexpr const char* kGroup = "239.1.1.1";
constexpr std::uint16_t kFeedPort = 9002;
constexpr const char* kIface = "127.0.0.1";
constexpr const char* kRequestHost = "127.0.0.1";
constexpr std::uint16_t kRequestPort = 9001;
constexpr auto kRequestTimeout = std::chrono::milliseconds(200);

void drainBuffer(LocalOrderBook& book,
                 std::map<std::uint64_t, ItchMessage>& buffer,
                 std::uint64_t& expected)
{
    for (auto it = buffer.find(expected); it != buffer.end(); it = buffer.find(expected))
    {
        std::visit([&](auto&& m) { book.apply(m); }, it->second);
        buffer.erase(it);
        ++expected;
    }
}
}

int main()
{
    std::cout << std::unitbuf;

    MulticastReceiver rx{kGroup, kFeedPort, kIface};
    LocalOrderBook book;

    std::uint64_t expected = 1;
    std::map<std::uint64_t, ItchMessage> buffer;
    std::uint8_t buf[2048];

    std::uint64_t requestedStart = 0;
    auto requestedAt = std::chrono::steady_clock::now();

    std::cout << "market maker: joined " << kGroup << ":" << kFeedPort
              << ", waiting for feed...\n";

    while (true)
    {
        const std::size_t n = rx.receive(buf, sizeof(buf));

        if (n > 0)
        {
            ByteReader r{buf, n};
            std::uint64_t seq = 0;
            std::uint16_t count = 0;
            bool ok = true;
            try
            {
                seq = r.u64();
                count = r.u16();
            }
            catch (const std::out_of_range&)
            {
                ok = false;
            }

            for (std::uint16_t i = 0; ok && i < count; ++i)
            {
                std::uint16_t len = 0;
                const std::uint8_t* msg = nullptr;
                try
                {
                    len = r.u16();
                    msg = r.take(len);
                }
                catch (const std::out_of_range&)
                {
                    break;
                }

                const std::uint64_t msgSeq = seq + i;
                if (msgSeq < expected)
                {
                    continue;
                }

                auto parsed = parseItch(msg, len);
                if (!parsed)
                {
                    std::cout << "[warn] unparseable message at seq " << msgSeq << "\n";
                    continue;
                }

                if (msgSeq == expected)
                {
                    std::visit([&](auto&& m) { book.apply(m); }, *parsed);
                    ++expected;
                }
                else
                {
                    buffer.emplace(msgSeq, std::move(*parsed));
                }
            }

            // sequences within a packet are consecutive, so drain once after
            drainBuffer(book, buffer, expected);

            const LocalOrderBook::L1 l1 = book.top();
            std::cout << "applied<=" << (expected - 1) << "  orders=" << book.orderCount()
                      << "  buffered=" << buffer.size();
            if (l1.bidPrice)
            {
                std::cout << "  bid " << *l1.bidPrice << " x " << l1.bidQty;
            }
            if (l1.askPrice)
            {
                std::cout << "  ask " << *l1.askPrice << " x " << l1.askQty;
            }
            std::cout << '\n';
        }

        if (!buffer.empty())
        {
            const std::uint64_t gapStart = expected;
            const std::uint64_t gapEnd = buffer.begin()->first;
            const auto now = std::chrono::steady_clock::now();
            const bool isNewGap = (gapStart != requestedStart);
            const bool timedOut = (now - requestedAt) > kRequestTimeout;

            if (isNewGap || timedOut)
            {
                const auto count = static_cast<std::uint16_t>(gapEnd - gapStart);
                const auto req = buildRequest(gapStart, count);
                rx.sendRequest(req.data(), req.size(), kRequestHost, kRequestPort);
                std::cout << "[req] " << (isNewGap ? "requesting" : "retrying")
                          << " seq " << gapStart << " count " << count << "\n";
                requestedStart = gapStart;
                requestedAt = now;
            }
        }
    }

    return 0;
}
