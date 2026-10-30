//
// Created by Curtis chang on 4/7/26.
//

#include "AsyncLogger.h"

#include "StockTicker/StockTicker.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>

namespace
{
const char* toString(LogEventType type)
{
    switch (type)
    {
        case LogEventType::ProtoParsed:
            return "ProtoParsed";
        case LogEventType::OrderAdded:
            return "OrderAdded";
        case LogEventType::OrderMatch:
            return "OrderMatch";
    }

    return "Unknown";
}

void writeTimestamp(std::ostream& output, const std::uint64_t timestampNs)
{
    using namespace std::chrono;

    const auto timestamp = time_point_cast<system_clock::duration>(
        system_clock::time_point{} + nanoseconds{timestampNs}
    );
    const std::time_t epochSeconds = system_clock::to_time_t(timestamp);
    const std::uint64_t nanosecondPart = timestampNs % 1'000'000'000;

    std::tm time{};
    gmtime_r(&epochSeconds, &time);

    output << std::put_time(&time, "%Y-%m-%dT%H:%M:%S")
           << '.'
           << std::setw(9)
           << std::setfill('0')
           << nanosecondPart
           << std::setfill(' ')
           << 'Z';
}
}

AsyncLogger::AsyncLogger(std::string outputPath, std::size_t queueCapacity)
    : queue_(queueCapacity),
      running_(true)
{
    const std::filesystem::path path{outputPath};
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path());
    }

    output_.open(path);
    worker_ = std::thread([this]
    {
        this->run();
    });
}

AsyncLogger::~AsyncLogger()
{
    running_.store(false, std::memory_order::release);
    if (worker_.joinable())
    {
        worker_.join();
    }
    drainBatch();
    output_ << "dropped_logs," << droppedCount() << '\n';
}

bool AsyncLogger::tryLog(const LogEvent& event)
{
    if (!queue_.try_push(event))
    {
        droppedCount_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    return true;
}

std::uint64_t AsyncLogger::droppedCount() const
{
    return droppedCount_.load(std::memory_order_relaxed);
}

void AsyncLogger::run()
{
    while (running_.load(std::memory_order::acquire))
    {
        drainBatch();
        std::this_thread::yield();
    }
}

void AsyncLogger::drainBatch()
{
    LogEvent event{};
    while (queue_.try_pop(event))
    {
        writeEvent(event);
    }
}

void AsyncLogger::writeEvent(const LogEvent& event)
{
    writeTimestamp(output_, event.timestampNs);
    output_ << ','
            << toString(event.type) << ','
            << toString(static_cast<StockTicker>(event.symbolId)) << ','
            << event.priceTicks << ','
            << event.qty << ','
            << event.latency << "ns\n";
}
