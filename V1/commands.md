# V1 Commands

Run these commands from the V1 project directory:

```bash
cd "/Users/curtischang/Documents/C++/OrderBook/V1"
```

The commands below use CLion's bundled CMake because `cmake` is not currently on
the shell `PATH`:

```text
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake
```

## Prerequisites

Confirm the local protobuf tools are available:

```bash
protoc --version
pkg-config --modversion protobuf
```

Python dependencies are managed by `uv` from `market_data_feeder/pyproject.toml`.

## Regenerate Protobuf Code

Generated C++ and Python bindings are already committed. Regenerate them after
changing `proto/order.proto`:

```bash
protoc \
  --cpp_out=generated \
  --python_out=market_data_feeder/generated \
  -I proto \
  proto/order.proto
```

## Debug Build

Configure:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
  -S . \
  -B cmake-build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile the server:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
  --build cmake-build-debug \
  --target OrderBook
```

Run the server:

```bash
./cmake-build-debug/OrderBook
```

## Release Build

Configure:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
  -S . \
  -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile the server:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
  --build cmake-build-release \
  --target OrderBook
```

Run the server:

```bash
./cmake-build-release/OrderBook
```

## Debug Versus Release

Debug builds are for correctness work:

- little or no compiler optimization
- easier source-level debugging
- variables and stack frames are easier to inspect
- assertions remain enabled
- slower and unsuitable for performance conclusions

Release builds are for realistic performance and profiling:

- compiler optimization is enabled
- functions may be inlined or removed
- assertions are normally disabled by `NDEBUG`
- much faster, but source-level debugging is less direct

Use Debug while fixing behavior. Use Release for latency measurements, throughput
benchmarks, and Apple Instruments.

## Unit Tests

Build the test executable using the Debug configuration:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
  --build cmake-build-debug \
  --target OrderBookTests
```

Run all registered tests:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/ctest \
  --test-dir cmake-build-debug \
  --output-on-failure
```

Run the test executable directly:

```bash
./cmake-build-debug/OrderBookTests
```

## Python Feeder

Start the C++ server first. In another terminal:

```bash
cd "/Users/curtischang/Documents/C++/OrderBook/V1/market_data_feeder"
uv run --project . python main.py
```

The feeder currently uses `fifty_thousand_data.txt`, as configured in `main.py`.

## Python Syntax Check

From `V1/market_data_feeder`:

```bash
uv run --project . python -m py_compile \
  main.py \
  encoder.py \
  market_data_feeder.py \
  message_source.py \
  transport.py \
  generated/order_pb2.py
```

## Python Profiling

From `V1/market_data_feeder`, with the C++ server already running:

```bash
uv run --project . python -m cProfile -o feeder.prof main.py
uv run --project . python -m pstats feeder.prof
```

## Apple Instruments

Build Release first, then launch Instruments and select:

```text
/Users/curtischang/Documents/C++/OrderBook/V1/cmake-build-release/OrderBook
```

Use the Time Profiler template and run the Python feeder from another terminal.

