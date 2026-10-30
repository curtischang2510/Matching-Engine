# Order Book V2

> This is an **exchange** (matching engine): it receives order flow from `order_flow/` and matches it. It is not a market maker — that is V3's C++ side.

V2 starts from the V1 implementation and is intended for targeted optimization work.

The purpose of this version is to keep the V1 behavior understandable while creating a separate place to experiment with performance improvements. The goal is not to make random micro-optimizations. Each change should be motivated by measurement, written clearly, and verified against the existing behavior.

## Observations of V1

1. Deque resizing is taking significant CPU cycles but `std::deque` does not have a `capacity` like `std::vector`

- Proposed Solution: Use a ring buffer that allows for either resizing or dropping orders when they have reached certain quantity. Make sure the OS is actually allocating and not lazily allocating

2. Noticed that the program is always running on difference CPUs. This could affect performance due to caching

- Proposed Solution: Set CPU affinity

3. Hashing takes significant CPU cycles

- Proposed Solution: Experiment creating custom hashing function or using if statements to see whichever is faster

## Working Rules

- Keep V1 available as the readable baseline.
- Measure before and after each meaningful optimization.
- Prefer one optimization theme per commit so performance changes are easy to review.
- Keep correctness tests passing while optimizing.
- Avoid changing multiple layers at once unless the measurement clearly shows the boundary is the bottleneck.
- Document any tradeoff where code becomes less simple for the sake of speed.

## Things To Measure

- Time spent reading frames from the socket.
- Time spent deserializing protobuf messages.
- Time spent converting protobuf orders into internal `Trade` objects.
- Time spent matching orders inside `OrderBook`.
- Time spent routing orders inside `Exchange`.
- Allocation count and allocation hotspots.
- Cache behavior when Instruments exposes useful counters.

## Build

Use a fresh build directory for V2 so it does not reuse any CMake cache from V1.

```bash
cd /Users/curtischang/Documents/C++/OrderBook/V2

cmake -S . -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang

cmake --build cmake-build-release --target OrderBook
```

## Test

```bash
cd /Users/curtischang/Documents/C++/OrderBook/V2

cmake --build cmake-build-release --target OrderBookTests
ctest --test-dir cmake-build-release --output-on-failure
```
