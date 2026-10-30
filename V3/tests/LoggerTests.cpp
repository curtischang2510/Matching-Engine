//
// Created by Curtis chang on 5/7/26.
//

#include "../Logger/AsyncLogger.h"
#include "../Logger/LogEvent.h"
#include "../Logger/SPSCBuffer.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

LogEvent make_event(const std::uint32_t symbolId)
{
    return LogEvent{
        .timestampNs = 100,
        .priceTicks = 10100,
        .symbolId = symbolId,
        .qty = 10,
        .latency = 5,
        .type = LogEventType::OrderAdded,
    };
}

void test_buffer_rejects_unusable_capacity()
{
    bool rejected = false;
    try
    {
        SPSCBuffer buffer{1};
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }

    assert(rejected);
}

void test_buffer_empty_full_and_fifo_invariant()
{
    SPSCBuffer buffer{3};
    LogEvent out{};

    assert(!buffer.try_pop(out));

    assert(buffer.try_push(make_event(1)));
    assert(buffer.try_push(make_event(2)));
    assert(!buffer.try_push(make_event(3)));

    assert(buffer.try_pop(out));
    assert(out.symbolId == 1);

    assert(buffer.try_pop(out));
    assert(out.symbolId == 2);

    assert(!buffer.try_pop(out));
}

void test_buffer_wraparound_preserves_fifo()
{
    SPSCBuffer buffer{3};
    LogEvent out{};

    assert(buffer.try_push(make_event(1)));
    assert(buffer.try_push(make_event(2)));

    assert(buffer.try_pop(out));
    assert(out.symbolId == 1);

    assert(buffer.try_push(make_event(3)));
    assert(!buffer.try_push(make_event(4)));

    assert(buffer.try_pop(out));
    assert(out.symbolId == 2);

    assert(buffer.try_pop(out));
    assert(out.symbolId == 3);

    assert(!buffer.try_pop(out));
}

void test_async_logger_writes_dropped_count_on_destruction()
{
    const std::filesystem::path logPath =
        std::filesystem::temp_directory_path() /
        ("orderbook_logger_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ) + ".log");
    std::filesystem::remove(logPath);

    {
        AsyncLogger logger{logPath.string(), 3};
        logger.tryLog(make_event(1));
    }

    std::ifstream input{logPath};
    assert(input.is_open());

    bool sawDroppedCount = false;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.rfind("dropped_logs,", 0) == 0)
        {
            sawDroppedCount = true;
            break;
        }
    }

    assert(sawDroppedCount);
    std::filesystem::remove(logPath);
}

int main()
{
    test_buffer_rejects_unusable_capacity();
    test_buffer_empty_full_and_fifo_invariant();
    test_buffer_wraparound_preserves_fifo();
    test_async_logger_writes_dropped_count_on_destruction();

    return 0;
}
