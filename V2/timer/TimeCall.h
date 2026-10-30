#ifndef ORDERBOOK_TIMER_TIMECALL_H
#define ORDERBOOK_TIMER_TIMECALL_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

template <typename Func>
auto timeCall(Func&& func, std::uint32_t& latencyNs)
{
    using Result = std::invoke_result_t<Func>;
    static_assert(!std::is_void_v<Result>, "timeCall does not support void callables");

    const auto start = std::chrono::steady_clock::now();
    auto result = std::invoke(std::forward<Func>(func));
    const auto end = std::chrono::steady_clock::now();

    const auto measuredNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    latencyNs = measuredNs == 0 ? 1 : static_cast<std::uint32_t>(measuredNs);

    return result;
}

#endif
