//
// Created by Curtis chang on 4/7/26.
//

#ifndef ORDERBOOK_ASYNCLOGGER_H
#define ORDERBOOK_ASYNCLOGGER_H
#include <atomic>
#include <cstddef>
#include <fstream>
#include <string>
#include <thread>

#include "LogEvent.h"
#include "SPSCBuffer.h"


class AsyncLogger {
public:
    AsyncLogger(std::string outputPath, std::size_t queueCapacity);
    ~AsyncLogger();
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    bool tryLog(const LogEvent& event);

    [[nodiscard]] std::uint64_t droppedCount() const;
private:
    void run();
    void drainBatch();
    void writeEvent(const LogEvent& event);

    SPSCBuffer        queue_;
    std::ofstream     output_;
    std::thread       worker_;
    std::atomic<bool> running_{true};
    std::atomic<std::uint64_t> droppedCount_{0};
};

#endif //ORDERBOOK_ASYNCLOGGER_H
