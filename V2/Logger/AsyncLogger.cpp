//
// Created by Curtis chang on 4/7/26.
//

#include "AsyncLogger.h"

#include <filesystem>

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
    output_ << event.timestampNs << ','
            << static_cast<int>(event.type) << ','
            << event.symbolId << ','
            << event.priceTicks << ','
            << event.qty << ','
            << event.latency << '\n';
}
