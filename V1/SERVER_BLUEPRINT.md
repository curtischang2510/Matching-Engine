# C++ Order Book Server Blueprint

## Goal

Build a C++ server process that receives market data messages over localhost,
parses them into `Trade` objects, sends them into `Exchange`, and optionally
returns an `ExecutionReport` response.

The important rule:

```text
Networking code should not enter OrderBook.
```

`OrderBook` should remain a domain object. It should not know about sockets,
ports, byte buffers, newline delimiters, or TCP.

## Recommended First Version

Start with one single-threaded TCP server.

This matches the current project stage:

- It is easy to reason about.
- It works with the Python feeder.
- It teaches socket lifecycle clearly.
- It avoids concurrency before the matching logic and parsing are tested.

The first server only needs to handle:

- One listening socket.
- One accepted client connection.
- Newline-delimited text messages.
- One `Exchange` instance.
- One message at a time.

## Message Flow

```text
Python feeder
  -> TCP socket bytes
  -> TcpServer
  -> LineReader
  -> TradeParser
  -> Exchange::addOrder
  -> ExecutionReportSerializer
  -> TCP socket bytes back to feeder
```

Each step has one job.

## Proposed File Layout

```text
OrderBook/
  main.cpp
  Exchange.h
  Exchange.cpp
  OrderBook.h
  OrderBook.cpp
  Trade.h

  Networking/
    TcpServer.h
    TcpServer.cpp
    SocketHandle.h
    LineReader.h
    LineReader.cpp
    TradeParser.h
    TradeParser.cpp
    ExecutionReportSerializer.h
    ExecutionReportSerializer.cpp
    OrderServer.h
    OrderServer.cpp
```

Do not create every file at once if that feels heavy. Start with fewer files and
split when responsibilities become clear. But these are the boundaries to aim
for.

## Class Responsibilities

## First RAII Milestone: Connect And Print

Before parsing orders, build the smallest useful server:

```text
C++ server starts
  -> creates listening socket
  -> binds to localhost:8001
  -> listens
  -> accepts one Python feeder connection
  -> sends "CONNECTED\n"
  -> closes sockets automatically through RAII
```

The goal is not to process market data yet. The goal is to prove:

- The Python feeder can connect.
- The C++ server can accept the connection.
- The server can send one small response.
- Socket cleanup happens automatically when objects leave scope.

### Minimal Files For This Milestone

Start with:

```text
Networking/
  SocketHandle.h
  TcpServer.h
  TcpServer.cpp
```

Then update:

```text
main.cpp
CMakeLists.txt
```

Do not add `TradeParser`, `LineReader`, or `OrderServer` yet. Those come after
connectivity works.

### `SocketHandle` For RAII

`SocketHandle` should be a tiny ownership class around an `int` file descriptor.

Its responsibility:

```text
Close the socket exactly once when the object dies.
```

It should support:

- Construction from a valid socket fd.
- Destructor closes the fd if it is valid.
- Copying disabled.
- Moving allowed later, or disabled for the first draft.
- A method to access the raw fd for socket system calls.

This is the C++ RAII idea:

```text
Constructor receives resource.
Destructor releases resource.
Normal scope rules guarantee cleanup.
```

Do not use `std::unique_ptr` for the socket fd. A file descriptor is an integer
owned by the operating system, not a heap allocation. A custom RAII wrapper is
the clearer tool.

### `TcpServer` For Socket Setup

`TcpServer` should own the listening socket.

Its responsibility:

```text
Prepare a TCP server socket and accept one client.
```

For this milestone, it can expose two simple operations:

```text
listenOn(host, port)
acceptClient()
```

Or it can do setup in the constructor:

```text
TcpServer server{"127.0.0.1", 8001};
SocketHandle client = server.acceptClient();
```

The second shape is cleaner once you are comfortable with constructors because a
constructed `TcpServer` is ready to use.

### What `main.cpp` Should Do For This Milestone

Keep `main.cpp` small:

```text
create TcpServer on localhost:8001
wait for one client
send "CONNECTED\n" to the accepted client
print one server-side message
return from main
```

When `main` returns:

- The accepted client socket wrapper is destroyed.
- The listening socket wrapper is destroyed.
- Both destructors close their file descriptors.

That is the behavior you want to observe.

### Python Feeder Change For This Milestone

The current Python `TcpTransport` can send but does not yet receive. Add one
small method later:

```text
receive(max_bytes) -> bytes
```

Then the feeder can:

```text
connect
receive greeting
print greeting
send messages later
close through context manager
```

Keep this temporary. Once the full server exists, responses should come from
`ExecutionReportSerializer`.

### Success Criteria

- [ ] Start C++ server.
- [ ] Start Python feeder.
- [ ] Python no longer gets `ConnectionRefusedError`.
- [ ] Python prints `CONNECTED`.
- [ ] C++ server exits cleanly after one connection.
- [ ] No manual `close` calls are needed in `main.cpp`.

Only after this works should you add parsing.

### `SocketHandle`

Owns a raw file descriptor and closes it in the destructor.

This is where C++ RAII matters. Sockets are operating system resources. If you
open one, you must close it even when errors happen.

Responsibility:

```text
Represent ownership of one socket file descriptor.
```

It should:

- Store an `int fd`.
- Close the fd in its destructor.
- Be non-copyable.
- Be movable later if needed.
- Expose the raw fd only when lower-level socket calls need it.

Why not use a smart pointer?

For a socket fd, `std::unique_ptr` is not the natural fit because the resource is
an integer, not a heap object. A small RAII wrapper class is cleaner.

### `TcpServer`

Owns server socket setup.

Responsibility:

```text
Bind, listen, and accept TCP client connections.
```

It should:

- Create the socket.
- Set useful socket options.
- Bind to host and port.
- Listen for connections.
- Accept one client.

For the first version, it can be blocking.

Important socket options to learn:

- `SO_REUSEADDR`: makes local development easier after restarting.
- `TCP_NODELAY`: disables Nagle's algorithm for lower latency on small messages.

### `LineReader`

Converts TCP byte stream data into complete messages.

TCP does not preserve message boundaries. If the feeder sends:

```text
AAPL,BID,100.50,10\n
```

the server might receive it as:

```text
AAPL,BID
```

then later:

```text
,100.50,10\n
```

Or it might receive multiple messages together.

Responsibility:

```text
Read bytes until a complete newline-delimited message is available.
```

It should:

- Maintain an internal buffer.
- Read from the socket.
- Return one complete line at a time.
- Preserve leftover bytes for the next call.

This class is one of the most important networking lessons in the project.

### `TradeParser`

Converts text into a `Trade`.

Input:

```text
AAPL,BID,100.50,10
```

Output:

```text
Trade{stockTicker="AAPL", orderType=OrderType::BID, price=100.50, qty=10}
```

Responsibility:

```text
Validate and parse one protocol message into a domain object.
```

It should reject:

- Missing fields.
- Unknown side.
- Non-numeric price.
- Non-numeric quantity.
- Zero or negative quantity.

For a first version, returning `std::optional<Trade>` is acceptable:

```cpp
std::optional<Trade> parseTrade(std::string_view line);
```

Later, if you want better error reporting, return a richer result type.

### `ExecutionReportSerializer`

Converts `ExecutionReport` into text to send back to the feeder.

Responsibility:

```text
Serialize the result of Exchange::addOrder into a wire response.
```

Example output:

```text
AAPL,BID,FILLS=1,100.00:5\n
```

For the first version, keep it simple. The feeder mainly needs to know that the
server received and processed the message.

Possible first responses:

```text
OK\n
ERROR,bad message\n
FILL,AAPL,BID,100.00,5\n
```

Do not overbuild this protocol yet.

### `OrderServer`

Coordinates the server workflow.

Responsibility:

```text
Glue networking, parsing, exchange handling, and response serialization together.
```

It owns:

- `TcpServer`
- `Exchange`
- `TradeParser`
- `ExecutionReportSerializer`

It should not contain detailed parsing code or low-level socket setup code.

Conceptual shape:

```text
OrderServer::run()
  accept client
  while client is connected:
    read one line
    parse Trade
    if parse fails:
      send error response
      continue
    report = exchange.addOrder(trade)
    response = serialize(report)
    send response
```

## Where `main.cpp` Fits

`main.cpp` should stay small.

Its job:

- Choose host and port.
- Construct `OrderServer`.
- Call `run()`.

Conceptually:

```text
int main()
{
    OrderServer server{"127.0.0.1", 8001};
    server.run();
}
```

Avoid putting parsing, socket loops, or order book logic directly in `main.cpp`.

## TCP First, UDP Later

To make UDP easier later, separate the server into two ideas:

```text
Transport receives/sends raw messages.
OrderServer processes complete messages.
```

For TCP, the transport needs `LineReader` because TCP is a byte stream.

For UDP, each datagram already has message boundaries, so a UDP receiver can
return one message per packet.

Future interface idea:

```cpp
class MessageTransport {
public:
    virtual ~MessageTransport() = default;
    virtual std::optional<std::string> receiveMessage() = 0;
    virtual void sendMessage(std::string_view message) = 0;
};
```

Then:

```text
TcpMessageTransport -> uses socket + LineReader
UdpMessageTransport -> uses datagrams
OrderServer         -> uses MessageTransport
```

Do not start with this abstraction if it slows you down. But keep the boundary in
mind: `OrderServer` should consume complete strings, not raw TCP chunks.

## Error Handling Strategy

For the first version:

- If `bind` fails, print an error and exit.
- If `listen` fails, print an error and exit.
- If `accept` fails, print an error and exit.
- If one message fails to parse, send an error response and keep running.
- If the client disconnects, exit cleanly.

Do not use exceptions everywhere at first. It is okay for low-level setup
functions to return failure or throw, but be consistent.

Good learning rule:

```text
Bad server setup is fatal. Bad client message is not fatal.
```

## Performance Notes

Do not chase ultra-low latency in the first server. First make the behavior
correct and measurable.

Low-latency-friendly choices for the first version:

- Use localhost.
- Use `TCP_NODELAY`.
- Avoid printing inside the hot message loop.
- Avoid sleeping in the server.
- Parse into `Trade` directly.
- Keep one `Exchange` alive for the whole server lifetime.
- Reuse buffers in `LineReader` where practical.

What not to do yet:

- No async I/O.
- No multithreading.
- No lock-free queues.
- No custom allocators.
- No binary protocol until text protocol works.

Later measurements should separate:

- Time spent receiving bytes.
- Time spent framing messages.
- Time spent parsing.
- Time spent in `Exchange::addOrder`.
- Time spent serializing responses.
- End-to-end feeder-to-server round trip.

## Testing Plan

Test in layers.

### Parser Tests

- [ ] Parse a valid bid.
- [ ] Parse a valid ask.
- [ ] Reject missing fields.
- [ ] Reject unknown side.
- [ ] Reject non-numeric price.
- [ ] Reject zero quantity.

### Serializer Tests

- [ ] Serialize no-fill report.
- [ ] Serialize one-fill report.
- [ ] Serialize multiple-fill report.

### Order Server Tests

For the first version, manual integration testing is acceptable:

- [ ] Start C++ server.
- [ ] Run Python feeder.
- [ ] Confirm server receives a line.
- [ ] Confirm parser creates a `Trade`.
- [ ] Confirm `Exchange::addOrder` is called.
- [ ] Confirm feeder receives a response.

Automated socket tests can come later.

## Suggested Build Order

1. Create `TradeParser` and unit test it without sockets.
2. Create `ExecutionReportSerializer` and unit test it without sockets.
3. Create `SocketHandle`.
4. Create `TcpServer` that accepts one connection.
5. Create `LineReader`.
6. Create `OrderServer`.
7. Make `main.cpp` construct and run `OrderServer`.
8. Run Python feeder against the server.
9. Add timing measurements after correctness works.

This order avoids debugging sockets, parsing, and order book logic all at once.

## Key Design Principle

Each layer should receive the cleanest possible input:

```text
Socket layer receives bytes.
LineReader receives bytes and emits lines.
Parser receives one line and emits Trade.
Exchange receives Trade and emits ExecutionReport.
Serializer receives ExecutionReport and emits text.
Transport sends text bytes.
```

If a class starts doing work from more than one layer, split it.
