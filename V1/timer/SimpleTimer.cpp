#include "SimpleTimer.h"
//
// Created by Curtis chang on 14/6/26.
//
int64_t SimpleTimer::elapsedNano() const
{
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
}

int64_t SimpleTimer::elapsedMicro() const
{
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
}