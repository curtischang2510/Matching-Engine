# Protobuf TCP Testing

## Commands To Run

Run these from the V1 project directory:

```bash
cd /Users/curtischang/Documents/C++/OrderBook/V1
```

Generate protobuf files:

```bash
protoc --cpp_out=generated --python_out=../order_flow/generated -I proto proto/order.proto
```

Configure CMake:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake -S . -B cmake-build-debug
```

Build the C++ server:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake --build cmake-build-debug --target OrderBook
```

Run the C++ server:

```bash
./cmake-build-debug/OrderBook
```

Leave that terminal open. In a second terminal, run the Python feeder.

```bash
cd /Users/curtischang/Documents/C++/OrderBook/order_flow
uv run --project . python main.py
```

If you want to send one hardcoded protobuf message instead of using `data.txt`:

```bash
cd /Users/curtischang/Documents/C++/OrderBook/order_flow
uv run --project . python -c "from encoder import ProtobufOrderEncoder; from transport import TcpTransport; payload = ProtobufOrderEncoder().encode('AAPL,BID,100.50,10'); transport = TcpTransport('127.0.0.1', 8001); transport.connect(); transport.send(payload); transport.close(); print('sent protobuf order')"
```

Expected C++ server output:

```text
No normal stdout output is expected in the hot path.
Malformed protobuf messages are still reported on stderr.
```

Run unit tests:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake --build cmake-build-debug --target OrderBookTests
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/ctest --test-dir cmake-build-debug --output-on-failure
```

## Data File Format

The Python feeder still reads text lines from `data.txt`:

```text
AAPL,BID,100.50,10
AAPL,ASK,100.40,5
```

But it no longer sends that text directly over TCP.

It now converts each line into:

```text
[4-byte big-endian payload length][protobuf Order payload]
```

## Changes Implemented

Added protobuf schema:

- `proto/order.proto`

Generated protobuf bindings:

- `generated/order.pb.h`
- `generated/order.pb.cc`
- `order_flow/generated/order_pb2.py`
- `order_flow/generated/__init__.py`

Updated Python feeder:

- `order_flow/encoder.py`
  - Added `ProtobufOrderEncoder`.
  - Converts CSV-style text into protobuf bytes.
  - Adds a 4-byte length prefix before each protobuf payload.

- `order_flow/main.py`
  - Switched from `NewlineTextEncoder` to `ProtobufOrderEncoder`.

- shared root `order_flow/pyproject.toml`
  - Added `protobuf>=6.32.0`.

Added C++ protobuf receive path:

- `Networking/ProtobufFrameReader.h`
- `Networking/ProtobufFrameReader.cpp`
  - Reads exactly 4 bytes for message length.
  - Reads exactly N bytes for the protobuf payload.

- `Networking/OrderProtoAdapter.h`
- `Networking/OrderProtoAdapter.cpp`
  - Converts generated protobuf `orderbook::Order` into your domain `Trade`.
  - Can convert your `ExecutionReport` into protobuf later.

Updated C++ server:

- `main.cpp`
  - Accepts a TCP client.
  - Reads length-prefixed protobuf frames.
  - Parses each frame into `orderbook::Order`.
  - Converts the protobuf order into `Trade`.
  - Calls `Exchange::addOrder`.
  - Prints how many fills were generated.

Updated build:

- `CMakeLists.txt`
  - Uses `pkg-config` to find protobuf.
  - Compiles generated protobuf C++ files.
  - Links the protobuf and Abseil dependencies.

## Troubleshooting

If Python says `ConnectionRefusedError`, the C++ server is not running or is not
listening on port `8001`.

If Python says it cannot import `google.protobuf`, run through `uv`:

```bash
uv run --project . python main.py
```

If the C++ build cannot find protobuf, confirm this works:

```bash
protoc --version
pkg-config --libs protobuf
```

If the server says `bind failed`, port `8001` may already be in use, or your
environment may not allow binding sockets.
