# V1 Commands

From the repository root, enter the V1 project directory:

```bash
cd V1
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

If CMake reports that `PkgConfig::PROTOBUF` includes a nonexistent Abseil
directory after a Homebrew update, refresh the cached protobuf paths:

```bash
cmake -U '*PROTOBUF*' -S . -B cmake-build-debug
```

Use `cmake-build-release` instead for a Release build. The `-U` option removes
matching CMake cache entries so `pkg-config` can discover the installed paths
again; it does not delete source files or the build directory.

Python dependencies are managed by `uv` from the shared root
`../order_flow/pyproject.toml`.

## Regenerate Protobuf Code

Generated C++ and Python bindings are already committed. Regenerate them after
changing `proto/order.proto`:

```bash
protoc \
  --cpp_out=generated \
  --python_out=../order_flow/generated \
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
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile the server:

```bash
cmake \
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
cmake \
  -S . \
  -B cmake-build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang
```

Compile the server:

```bash
cmake \
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

Run the test executable directly:

```bash
./cmake-build-debug/OrderBookTests
```

## Python Feeder

Start the C++ server first. In another terminal:

```bash
cd ../order_flow
uv run --project . python main.py
```

The feeder is shared by all order book versions and uses the input path
configured in `main.py`.

## Python Syntax Check

From the shared root `order_flow` directory:

```bash
cd ../order_flow
uv run --project . python -m py_compile \
  main.py \
  encoder.py \
  market_data_feeder.py \
  message_source.py \
  transport.py \
  generated/order_pb2.py
```

## Python Profiling

From the shared root `order_flow` directory, with the C++ server
already running:

```bash
cd ../order_flow
uv run --project . python -m cProfile -o feeder.prof main.py
uv run --project . python -m pstats feeder.prof
```

## Apple Instruments

Build Release first, then launch Instruments and select:

```text
V1/cmake-build-release/OrderBook
```

Use the Time Profiler template and run the Python feeder from another terminal.
