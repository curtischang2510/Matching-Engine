# Profiling On Apple Silicon

## Goal

You want to answer two different questions:

1. Which part of the program is slow?
2. Why is that part slow?

These need different tools.

```text
Manual timers answer: how long did this code section take?
Profilers answer: where did CPU time go?
Counters answer: what hardware or OS events happened?
```

For this order book project, start with manual timing around your own pipeline,
then use Apple Instruments to inspect the hottest functions.

## Build Type Matters

Do not profile a debug build for performance conclusions.

Use a release build with debug symbols:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake -S . -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang

/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake --build cmake-build-release --target OrderBook
```

Recommended release flags later:

```cmake
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")
```

`-O3` optimizes.
`-g` keeps readable symbols for profilers.
`-DNDEBUG` disables `assert`.

## What To Measure In This Project

Your current server pipeline is:

```text
recv 4-byte length prefix
recv protobuf payload
protobuf ParseFromString
protobuf -> Trade adapter
Exchange::addOrder
optional response serialization
send response
```

Time each stage separately.

This will tell you whether the bottleneck is:

- socket I/O
- protobuf parsing
- conversion into your domain type
- the order book matching logic
- response creation
- logging

## Manual Timing

Manual timing is useful when you already know the sections you care about.

Use `std::chrono::steady_clock`, not wall-clock time.

### Simple Timer

```cpp
#include <chrono>
#include <cstdint>

class Timer {
public:
    Timer()
        : start_(std::chrono::steady_clock::now())
    {
    }

    std::int64_t elapsedNs() const
    {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};
```

Use it like:

```cpp
Timer timer;
// code you want to measure
std::int64_t ns = timer.elapsedNs();
```

Do not print `ns` every time inside a hot loop. Printing will dominate the
runtime.

## Aggregate Timing

Instead of printing every measurement, aggregate.

```cpp
#include <algorithm>
#include <cstdint>
#include <limits>

struct TimingStats {
    std::uint64_t count = 0;
    std::int64_t totalNs = 0;
    std::int64_t minNs = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxNs = 0;

    void record(std::int64_t ns)
    {
        ++count;
        totalNs += ns;
        minNs = std::min(minNs, ns);
        maxNs = std::max(maxNs, ns);
    }

    double averageNs() const
    {
        if (count == 0)
        {
            return 0.0;
        }

        return static_cast<double>(totalNs) / static_cast<double>(count);
    }
};
```

For your server, keep one `TimingStats` per stage:

```cpp
struct ServerTiming {
    TimingStats readFrame;
    TimingStats parseProto;
    TimingStats adaptTrade;
    TimingStats addOrder;
};
```

Then print the summary only at the end.

## RAII Scoped Timing

A scoped timer is useful when you want a timer that records automatically at the
end of a scope.

```cpp
#include <chrono>

class ScopedTimer {
public:
    explicit ScopedTimer(TimingStats& stats)
        : stats_(stats),
          start_(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTimer()
    {
        auto end = std::chrono::steady_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        stats_.record(ns);
    }

private:
    TimingStats& stats_;
    std::chrono::steady_clock::time_point start_;
};
```

Usage:

```cpp
{
    ScopedTimer timer(timing.parseProto);
    order.ParseFromString(payload);
}
```

This is RAII again:

```text
constructor starts timing
destructor records timing
scope controls lifetime
```

## Watch Out For Timer Overhead

Manual timers have overhead.

For large sections like protobuf parsing or full order processing, this overhead
is usually acceptable.

For extremely tiny functions, timing each call can distort results. In that case:

- run the function many times
- time the whole batch
- divide by the number of iterations

Example:

```cpp
Timer timer;
for (int i = 0; i < 1'000'000; ++i)
{
    // code under test
}
auto avgNs = timer.elapsedNs() / 1'000'000.0;
```

## Percentiles

Averages hide tail latency.

For trading-style systems, eventually track:

- p50
- p90
- p95
- p99
- max

Simple approach:

- store per-message latency in `std::vector<std::int64_t>`
- sort at the end
- index into percentiles

Do not store every stage forever in a real system, but it is fine for learning.

## Apple Instruments

Apple Instruments is the main GUI profiler on macOS. Start with **Time
Profiler**.

Use it to answer:

```text
Which functions are consuming CPU time?
```

Workflow:

1. Build `cmake-build-release/OrderBook`.
2. Open Instruments.
3. Choose **Time Profiler**.
4. Select or launch your `OrderBook` binary.
5. Start recording.
6. Run the Python feeder with many messages.
7. Stop recording.
8. Inspect the call tree.

Look for hot functions such as:

- protobuf parsing
- `Exchange::addOrder`
- `OrderBook::matchIncomingBid`
- `OrderBook::matchIncomingAsk`
- `std::map` operations
- string allocation/copying
- logging

Useful views:

- Call Tree
- Invert Call Tree
- Hide System Libraries
- Separate by Thread

Start with inverted call tree. It shows leaf functions where time is actually
spent.

Apple Instruments help is available at:

```text
https://help.apple.com/instruments/mac/current/
```

## Command-Line Instruments With `xctrace`

You can record from the command line with `xctrace`.

Example:

```bash
xcrun xctrace record \
  --template "Time Profiler" \
  --output orderbook.trace \
  --launch -- ./cmake-build-release/OrderBook
```

Then run the feeder in another terminal.

Open the trace afterward:

```bash
open orderbook.trace
```

This gives you a repeatable way to collect traces.

## perf-Like Tools On macOS

Linux has `perf`. macOS does not provide the same `perf` workflow.

Closest tools:

- Instruments Time Profiler: CPU sampling.
- Instruments System Trace: scheduling, syscalls, thread activity.
- Instruments Allocations: heap allocation behavior.
- Instruments Counters: hardware-counter style profiling when available.
- `sample`: quick stack sampling from Terminal.
- `vm_stat`: system VM/page statistics.
- `xctrace`: command-line Instruments recording.

For a fast terminal sample:

```bash
sample <pid> 10 -file sample.txt
```

This samples a process for 10 seconds and writes stack information.

It is less rich than Instruments, but useful for a quick check.

## Page Faults

Page faults happen when the CPU accesses a virtual memory page that is not
currently mapped in the way it needs.

Types:

- Minor/soft page fault: resolved without disk I/O.
- Major/hard page fault: requires disk I/O.

For your order book, major page faults should not be in the hot path after
startup. If they are, something is wrong with workload setup or memory pressure.

Ways to inspect:

- Instruments System Trace.
- Instruments VM/memory tools.
- `vm_stat` before and after a benchmark.

Example:

```bash
vm_stat
```

Run it before and after a large replay. Look at changes in pageins/pageouts and
fault counters.

This is coarse process-external information. Instruments gives better timeline
context.

## Cache Misses

Cache misses happen when data is not in the CPU cache level needed by the load.

For an order book, cache misses may come from:

- pointer-heavy containers like `std::map`
- heap-allocated `std::deque` blocks
- string allocations
- protobuf allocation/parsing
- unordered map lookup in `Exchange`

On Apple Silicon, hardware counter availability in Instruments can vary by OS,
Xcode version, permissions, and template. Look for **Counters** or hardware
counter instruments in Instruments.

If you can record counters, useful metrics include:

- CPU cycles
- instructions
- cache misses
- branch misses
- memory loads/stores

If hardware counters are not available, use Time Profiler plus code changes:

- compare `std::map` with flatter structures later
- reduce allocations
- batch parse messages
- use integer prices instead of double/string-heavy parsing

## Allocation Profiling

Allocations are often the first bottleneck before true hardware limits.

Use Instruments **Allocations** to answer:

```text
Where are heap allocations happening?
```

Likely sources in your project:

- protobuf `ParseFromString`
- `std::string` ticker copies
- `std::vector<Fill>`
- `std::map` node allocations
- `std::deque` blocks

If allocations are hot, possible improvements later:

- use integer ticker ids
- avoid repeated string copies
- reuse buffers
- reserve vectors
- use simpler data structures

Do not optimize these before measuring.

## Benchmark Setup

Use a repeatable input file.

Example:

```text
AAPL,ASK,100.00,10
AAPL,BID,101.00,5
AAPL,BID,100.50,20
```

For meaningful profiling, generate a large file:

```text
100k messages minimum
1M messages if you want stable profiles
```

Avoid printing per message in both Python and C++.

Your Python feeder currently prints each message. Remove that before measuring
seriously.

## Recommended Profiling Sequence

1. Run correctness tests.
2. Build Release.
3. Disable per-message logging.
4. Add aggregate timers around server stages.
5. Run a fixed replay file.
6. Print timing summary at shutdown.
7. Run Instruments Time Profiler.
8. Check whether manual timing and profiler agree.
9. Run Allocations if parsing or string code looks hot.
10. Look for counters/page faults only after CPU and allocation profiles are
    understood.

## What Good First Results Look Like

After a benchmark, you want a table like:

```text
stage                  count      avg_ns      min_ns      max_ns
read_frame             100000     1200        400         50000
parse_protobuf         100000     900         300         20000
adapt_trade            100000     100         50          5000
exchange_add_order     100000     700         200         30000
```

This tells you where to look next.

If `read_frame` dominates, inspect socket behavior and feeder pacing.

If `parse_protobuf` dominates, inspect protobuf allocations or binary schema.

If `exchange_add_order` dominates, inspect order book data structures.

If logging dominates, remove logging from the hot path.

## Common Profiling Mistakes

- Profiling debug builds.
- Printing inside the loop.
- Measuring tiny functions one call at a time.
- Changing multiple things between runs.
- Using too few messages.
- Trusting one run.
- Ignoring correctness after optimization.
- Optimizing code that is not hot.

## Near-Term Plan For This Project

Add timing first around:

- `ProtobufFrameReader::readFrame`
- `order.ParseFromString`
- `toTrade`
- `exchange.addOrder`

Then run Instruments Time Profiler on the release binary.

Only after that should you decide whether to optimize protobuf parsing, socket
I/O, or order book data structures.

## Python Feeder Profiling

You should profile the Python feeder too. Otherwise you cannot tell whether the
C++ server is slow or the Python sender is simply not feeding it fast enough.

Your Python feeder pipeline is:

```text
read line from file
parse CSV text
build protobuf Order
SerializeToString
create length prefix
sendall over socket
```

Time these stages separately before assuming the server is the bottleneck.

## Python Manual Timing

Use `time.perf_counter_ns()` for high-resolution timing.

Example shape:

```python
import time

start = time.perf_counter_ns()
# code to measure
elapsed_ns = time.perf_counter_ns() - start
```

As in C++, do not print every measurement inside the hot loop. Aggregate.

```python
class TimingStats:
    def __init__(self) -> None:
        self.count = 0
        self.total_ns = 0
        self.min_ns = None
        self.max_ns = 0

    def record(self, ns: int) -> None:
        self.count += 1
        self.total_ns += ns
        self.min_ns = ns if self.min_ns is None else min(self.min_ns, ns)
        self.max_ns = max(self.max_ns, ns)

    def avg_ns(self) -> float:
        if self.count == 0:
            return 0.0
        return self.total_ns / self.count
```

Useful feeder timing buckets:

```python
read_message
encode_protobuf
send_socket
total_message
```

The most important one is `send_socket`. If `sendall` is slow, the Python feeder
may be blocked by the OS socket buffer or by the C++ server not reading quickly
enough.

## Python Scoped Timer

Python has context managers, so you can make a scoped timer similar to C++ RAII:

```python
import time

class ScopedTimer:
    def __init__(self, stats: TimingStats) -> None:
        self.stats = stats

    def __enter__(self):
        self.start_ns = time.perf_counter_ns()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        self.stats.record(time.perf_counter_ns() - self.start_ns)
        return False
```

Usage:

```python
with ScopedTimer(timing.encode_protobuf):
    payload = encoder.encode(message)
```

This mirrors the C++ scoped timer:

```text
enter starts timing
exit records timing
with block controls lifetime
```

## Built-In Python Profilers

Start with `cProfile`.

Run the feeder under `cProfile`:

```bash
cd /Users/curtischang/Documents/C++/OrderBook/order_flow
uv run --project . python -m cProfile -o feeder.prof main.py
```

Then inspect it:

```bash
uv run --project . python -m pstats feeder.prof
```

Inside `pstats`, useful commands:

```text
sort cumulative
stats 30
sort time
stats 30
```

Meaning:

- `cumulative`: includes time spent inside child calls.
- `time`: time spent directly in the function itself.

Look for:

- protobuf serialization functions
- `socket.sendall`
- file iteration
- string parsing/splitting
- printing

## py-spy

`py-spy` is a sampling profiler for Python. It is useful because it can profile a
running Python process with less code intrusion than manual timers.

If installed, usage looks like:

```bash
py-spy top --pid <python_pid>
```

or:

```bash
py-spy record -o feeder.svg -- python main.py
```

This gives a flamegraph. Use this later if `cProfile` is too intrusive or you
want a visual profile.

Do not add this dependency yet unless you need it. `cProfile` is built in.

## Separating Python Bottleneck From C++ Bottleneck

Run three experiments.

### Experiment 1: Encode Only

Do not connect to the server. Just read messages and encode them.

Goal:

```text
How fast can Python create protobuf payloads?
```

If this is slow, networking is not the first bottleneck.

### Experiment 2: Send To Server

Run the real feeder against the C++ server.

Goal:

```text
How long does encode + send take?
```

If `sendall` is much slower than encode-only, the feeder is probably waiting on
the socket or server.

### Experiment 3: C++ Server Read Only

Temporarily make the C++ server read frames and discard them without parsing or
calling `Exchange`.

Goal:

```text
How much cost is pure socket read/framing?
```

This separates network/framing overhead from protobuf parsing and order book
logic.

## Python Logging Warning

Your feeder currently prints each message.

```python
print(message)
```

Remove this before profiling. Printing per message can dominate the entire
runtime and make networking look slower than it is.

If you need progress output, print every N messages:

```python
if count % 100000 == 0:
    print(count)
```

## Python Benchmark Setup

Use a fixed input file and count messages.

For each run, report:

```text
messages sent
total seconds
messages per second
average encode ns
average send ns
```

Example output shape:

```text
messages: 100000
total_s: 1.42
messages_per_second: 70422
encode_avg_ns: 1800
send_avg_ns: 8500
```

If `send_avg_ns` dominates, check server speed and socket backpressure.

If `encode_avg_ns` dominates, check protobuf object creation and string parsing.

## Python Tools Summary

- `time.perf_counter_ns`: best for explicit stage timing.
- `cProfile`: built-in function-level profiler.
- `pstats`: built-in cProfile result viewer.
- `py-spy`: external sampling profiler and flamegraphs.
- Instruments: can observe Python process CPU behavior at the system level, but
  it will not understand Python functions as clearly as Python-specific tools.

Use manual timing first, then `cProfile`.
