# Networking Suggestion

## Recommendation

Use two separate programs:

- `orderbook_server`: a C++ process that owns the `Exchange` and receives market
  data messages over a local network socket.
- `order_flow`: a replay process that reads historical or generated
  orders from disk and sends them to the server over localhost.

This is the best fit for the project because it teaches networking without
putting file I/O inside the order book. The order book should not know whether an
order came from a file, a socket, a test, or a future real feed. It should only
receive parsed domain objects and update book state.

## Why Not Let The Order Book Read The File?

Letting `OrderBook` read directly from disk is simpler, but it teaches the wrong
boundary.

The matching engine should be responsible for:

- Accepting an order.
- Matching it against resting liquidity.
- Updating internal state.
- Returning fills or execution reports.

It should not be responsible for:

- Opening files.
- Parsing CSV lines.
- Managing socket buffers.
- Sleeping to simulate message timing.
- Retrying network operations.

Keeping these concerns separate makes the code easier to test. You can unit test
the order book by constructing `Trade` objects directly, then separately test the
network/feed layer.

## Why A Feeder Process Still Makes Sense

The feeder may read from disk, but that does not mean the order book is reading
from disk. The disk read happens outside the matching process.

The flow becomes:

```text
file or generator -> order_flow -> localhost socket -> orderbook_server -> Exchange -> OrderBook
```

This simulates the shape of a real system:

- A market data source produces messages.
- A process receives bytes over a network boundary.
- The receiver parses bytes into domain objects.
- The matching/order book logic consumes clean C++ structs.

The latency will not represent a real exchange, but the architecture teaches the
right separation.

## Transport Choice

Start with TCP first.

TCP is easier to reason about as a beginner because it gives you an ordered byte
stream. You will learn:

- Creating a socket.
- Binding to a port.
- Listening for a connection.
- Accepting a client.
- Reading bytes.
- Handling partial reads.
- Parsing message boundaries.

UDP is also useful later, especially because real market data feeds often use
UDP-style multicast patterns. But UDP adds packet loss, message boundaries, and
out-of-order thinking earlier than you need.

Recommended order:

- Phase A: in-process tests call `Exchange::addOrder` directly.
- Phase B: single C++ server accepts newline-delimited TCP messages.
- Phase C: feeder process reads a file and sends messages over localhost TCP.
- Phase D: add timestamps and replay speed.
- Phase E: experiment with UDP.

## Message Format

Use a simple text protocol first:

```text
AAPL,BID,100.50,10
AAPL,ASK,100.60,5
MSFT,BID,420.00,20
```

Each line means:

```text
ticker,side,price,quantity
```

This is not the fastest protocol, but it is good for learning because it is easy
to inspect, debug, and generate by hand.

Later, after the networking behavior works, you can compare it against a compact
binary message format.

## Proposed C++ Boundaries

Keep the order book API clean:

```text
Exchange::addOrder(Trade) -> ExecutionReport
```

Put networking concerns in separate classes or functions:

- `MarketDataServer`: owns socket setup and reads raw messages.
- `MessageParser`: converts one message string into a `Trade`.
- `MarketDataFeeder`: reads input data and sends messages to the server.

The important design rule:

```text
OrderBook should never include socket headers or file parsing logic.
```

## Python Or C++ Feeder?

Use C++ for the server because you want networking experience in the main
program.

For the feeder, either is defensible:

- Python feeder: faster to write, easier to generate test data, good for early
  experiments.
- C++ feeder: more C++ practice, better if you want both sides of the socket in
  C++.

Recommendation:

- Start with a Python feeder if your immediate goal is testing the C++ server.
- Rewrite the feeder in C++ later if your goal shifts to practicing client-side
  socket code.

This avoids getting stuck writing two networking programs before the server side
works.

## Latency And Disk

Reading from disk is not automatically bad if it happens in the feeder. It only
becomes a bad simulation if the matching engine itself blocks on disk I/O.

For a replay simulation, the feeder can read the whole file into memory first:

```text
startup: read file into memory
runtime: send preloaded messages over socket
```

That gives you repeatable input without making disk latency part of the hot
path.

Later, you can measure:

- File read time.
- Message parse time.
- Socket receive time.
- Time spent inside `Exchange::addOrder`.
- End-to-end time from feeder send to server response.

## Suggested Learning Milestones

- [ ] Finish correctness tests for `OrderBook` before adding networking.
- [ ] Add a small parser from one text line to `Trade`.
- [ ] Add a small serializer from `ExecutionReport` to text.
- [ ] Build a server that accepts one TCP client.
- [ ] Send one hardcoded message from a feeder.
- [ ] Feed several messages from a file.
- [ ] Return execution reports from server to feeder.
- [ ] Add timestamps to messages.
- [ ] Replay messages at a fixed rate.
- [ ] Measure parsing, networking, and matching time separately.
- [ ] Compare text messages with a binary message format.
- [ ] Try UDP after TCP is understandable.

## What To Avoid For Now

- Do not connect to a paid or external market data API yet.
- Do not add networking code inside `OrderBook`.
- Do not optimize latency before correctness and measurement exist.
- Do not start with async I/O, epoll/kqueue, multicast, or lock-free queues.
- Do not build a complex protocol before a simple line-based protocol works.

## Final Position

The cleanest next step is not real external market data. It is a local replay
system that behaves like market data from the order book's perspective.

Build the order book as a pure domain engine. Build the network server around
it. Build the feeder as a separate process. This gives you C++ networking
practice while preserving the clean design boundary that the order book project
is trying to teach.

