#ifndef ORDERBOOK_TIMER_SIMPLETIMER_H
#define ORDERBOOK_TIMER_SIMPLETIMER_H

#include <chrono>
#include <cstdint>

class SimpleTimer
{
public:
    SimpleTimer():start_(std::chrono::steady_clock::now()){}

    [[nodiscard]] int64_t elapsedNano() const;
    [[nodiscard]] int64_t elapsedMicro() const;

private:
    std::chrono::steady_clock::time_point start_;
};

#endif
