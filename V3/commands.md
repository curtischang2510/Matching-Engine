# V3 Commands

From the repository root, enter the V3 project directory:

```bash
cd V3
```

The commands assume `cmake` and `ctest` are available on your shell `PATH`.
If you use CLion's bundled CMake on an Apple Silicon Mac, add it locally in
`~/.zshrc` without committing a machine-specific path:

```bash
export PATH="$HOME/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin:$PATH"
```

Restart the terminal or run `source ~/.zshrc`, then verify with
`cmake --version`.

## Prerequisites

Confirm the local protobuf tools are available:

```bash
protoc --version
pkg-config --modversion protobuf
```

Python dependencies for the `exchange/` side are managed by `uv` from
`exchange/pyproject.toml`. Unlike V1/V2, this is V3's own copy, not the
shared top-level `order_flow/` — see `README.md` for why.

## Logging Build Option

V3 has a compile-time logging option:

```bash
-DORDERBOOK_ENABLE_LOGGING=ON
```

Logging is enabled by default. When it is enabled, `Exchange::addOrder` records
latency and writes async log events through `AsyncLogger`.

To compile the hot path without logging or timing code:

```bash
-DORDERBOOK_ENABLE_LOGGING=OFF
```

Because this is a CMake compile definition, changing it requires reconfiguring
the build directory before rebuilding.

## Regenerate Protobuf Code

Generated C++ and Python bindings are already committed. Regenerate them after
changing `proto/order.proto`:

```bash
protoc \
  --cpp_out=generated \
  --python_out=exchange/generated \
  -I proto \
  proto/order.proto
```

## Debug Build

Configure:

```bash
cmake \
  -S . \
  -B cmake-build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DORDERBOOK_ENABLE_LOGGING=ON \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile:

```bash
cmake \
  --build cmake-build-debug \
  --target MarketMaker
```

Run:

```bash
./cmake-build-debug/MarketMaker
```

## Release Build

Configure:

```bash
cmake \
  -S . \
  -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERBOOK_ENABLE_LOGGING=ON \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile:

```bash
cmake \
  --build cmake-build-release \
  --target MarketMaker
```

Run:

```bash
./cmake-build-release/MarketMaker
```

## Release Build Without Logging

Configure a separate build directory when comparing logging overhead:

```bash
cmake \
  -S . \
  -B cmake-build-release-no-logging \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERBOOK_ENABLE_LOGGING=OFF \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile:

```bash
cmake \
  --build cmake-build-release-no-logging \
  --target MarketMaker
```

Run:

```bash
./cmake-build-release-no-logging/MarketMaker
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

Build the test executables using the Debug configuration:

```bash
cmake \
  --build cmake-build-debug \
  --target OrderBookTests
```

Run all registered tests:

```bash
ctest \
  --test-dir cmake-build-debug \
  --output-on-failure
```

Run a test executable directly:

```bash
./cmake-build-debug/OrderBookTests
```

Note: `OrderBookTests`/`OrderBook.{h,cpp}` still carry their V2 names for now
— see `README.md` for the naming TODO once order-ID indexing lands.

## Python Exchange

Build the C++ market maker first. In another terminal:

```bash
cd exchange
uv run --project . python main.py
```

`exchange/` is V3's own copy — it no longer reads from the shared
`../order_flow/` root the way V1/V2 do (only the large historical
CSVs under `order_flow/data/` are still referenced from there, via a
relative path in `exchange/main.py`, to avoid duplicating hundreds of MB of
data).

## Python Syntax Check

From `V3/exchange`:

```bash
cd exchange
uv run --project . python -m py_compile \
  main.py \
  encoder.py \
  exchange.py \
  message_source.py \
  transport.py \
  generated/order_pb2.py
```

## Python Profiling

From `V3/exchange`, with the C++ market maker already running:

```bash
cd exchange
uv run --project . python -m cProfile -o exchange.prof main.py
uv run --project . python -m pstats exchange.prof
```

## Apple Instruments

Build Release first, then launch Instruments and select:

```text
V3/cmake-build-release/MarketMaker
```

Use the Time Profiler template and run the Python exchange from another terminal.
