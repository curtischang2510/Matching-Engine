# Low Latency Python Logging Plan

This note describes how to implement logging for the Python market data feeder without putting slow file I/O directly in the hot path.

The goal is not to make Python logging as fast as a custom C++ logger. The goal is to make Python logging good enough for feeder visibility while keeping most of the serious low-latency systems work on the C++ side.

## Problem

Normal logging can be expensive if it happens inside the per-message loop:

```text
read CSV row -> encode protobuf -> send TCP payload -> log to file
```

The expensive parts are:

- formatting the log message
- creating logging metadata
- acquiring locks inside the logging system
- writing to disk
- flushing buffers

The worst part is disk I/O. If the feeder thread writes to disk directly for every packet, the feeder can become bottlenecked by logging instead of by encoding or networking.

## Recommended Shape

Use Python's standard logging system with a queue:

```text
producer thread
  MarketDataFeeder / decorator
      -> logger.debug(...)
      -> QueueHandler
      -> SimpleQueue

background listener thread
      -> QueueListener
      -> FileHandler
      -> log file
```

The feeder thread only places a log record into a queue. A background thread pulls records from the queue and writes them to disk.

This is still not free. The feeder still pays for the function call, the enabled-level check, the `LogRecord` allocation, and the queue push. But it avoids doing file writes directly in the packet sending path.

## Terms

### Logger

A `Logger` is the object your code calls:

```python
logger.debug("encoded order in %d ns", elapsed_ns)
```

It decides whether a message should be emitted based on the log level.

Examples:

- `DEBUG`: detailed diagnostics
- `INFO`: normal high-level status
- `WARNING`: unusual but not fatal
- `ERROR`: something failed

For performance-sensitive code, the log level matters. If the logger level is `INFO`, then `logger.debug(...)` should be skipped before creating a full message.

### Handler

A `Handler` decides where a log record goes.

Examples:

- file
- console
- queue
- socket

A normal `FileHandler` writes to disk directly. That is simple but bad for a hot loop.

### QueueHandler

`QueueHandler` is a logging handler that sends log records into a queue instead of writing them immediately.

In this project, it should sit on the feeder thread side:

```text
logger -> QueueHandler -> queue
```

This reduces the latency impact of disk writes on the feeder thread.

### QueueListener

`QueueListener` runs in a background thread. It receives log records from the queue and forwards them to real handlers, such as a file handler.

In this project:

```text
queue -> QueueListener -> FileHandler
```

The listener should be started when the feeder starts and stopped when the feeder exits.

### SimpleQueue

`queue.SimpleQueue` is an unbounded queue with a simple API. It is a good first choice because `put()` does not normally block.

The tradeoff is that because it is unbounded, if logs are produced faster than the listener can write them, memory usage can grow. This is another reason not to log every packet.

### Decorator

A decorator wraps a function with extra behavior.

For timing:

```text
call wrapper
  record start time
  call original function
  record end time
  log elapsed time
  return original result
```

Your current decorator does the timing but discards the result. The next version should either:

- log the latency
- update an in-memory counter
- sample the latency occasionally

## What To Log

Do not log every market data message at first.

Prefer aggregate logging:

```text
sent=100000 encode_avg_ns=1200 send_avg_ns=8000 msgs_per_sec=45000
```

Useful feeder metrics:

- total messages sent
- total encode time
- average encode time
- total send time
- average send time
- rows skipped due to parse errors
- total elapsed runtime
- messages per second

Useful event logs:

- feeder started
- feeder stopped
- connected to C++ server
- connection failed
- bad CSV row
- protobuf encoding failure

## What Not To Log

Avoid this in the hot path:

```python
logger.debug(f"Sent order {message.symbol} {message.side} {message.price}")
```

The f-string is evaluated before the logger decides whether the message is needed.

Prefer:

```python
logger.debug("sent order symbol=%s side=%s price=%s", message.symbol, message.side, message.price)
```

This lets the logging library defer formatting until it knows the message will actually be emitted.

## Implementation Plan

### 1. Create a logging setup module

Create a module such as:

```text
market_data_feeder/logger/setup.py
```

It should expose one function:

```text
setup_async_logging(log_path, level) -> QueueListener
```

That function should:

- create a `SimpleQueue`
- create a `QueueHandler`
- attach the `QueueHandler` to the feeder logger
- create a `FileHandler`
- create a `QueueListener`
- return the listener so the caller can start and stop it

The caller should own the listener lifetime.

Example:

```python
# market_data_feeder/logger/setup.py

import logging
import queue
from logging.handlers import QueueHandler, QueueListener
from pathlib import Path


def setup_async_logging(log_path: Path, level: int = logging.INFO) -> QueueListener:
    log_path.parent.mkdir(parents=True, exist_ok=True)

    log_queue: queue.SimpleQueue[logging.LogRecord] = queue.SimpleQueue()
    queue_handler = QueueHandler(log_queue)

    file_handler = logging.FileHandler(log_path)
    file_handler.setLevel(level)
    file_handler.setFormatter(
        logging.Formatter(
            "%(asctime)s %(levelname)s %(name)s %(message)s"
        )
    )

    logger = logging.getLogger("market_data_feeder")
    logger.setLevel(level)
    logger.handlers.clear()
    logger.addHandler(queue_handler)
    logger.propagate = False

    return QueueListener(log_queue, file_handler, respect_handler_level=True)
```

Important points:

- `logger.handlers.clear()` prevents duplicate logs if setup runs more than once.
- `logger.propagate = False` prevents logs from also going to the root logger.
- `QueueHandler` is attached to the producer-side logger.
- `FileHandler` is attached to the background `QueueListener`.

### 2. Start and stop the listener in `main.py`

In `main.py`, the structure should become:

```text
listener = setup_async_logging(...)
listener.start()
try:
    feeder.run(...)
finally:
    listener.stop()
```

This is similar in spirit to RAII in C++, except Python uses `try/finally` or context managers to guarantee cleanup.

Since this project supports Python 3.9+, do not rely on `QueueListener` as a context manager unless you intentionally raise the project to Python 3.14.

Example:

```python
# market_data_feeder/main.py

import logging
from pathlib import Path

from encoder import ProtobufOrderEncoder
from logger.setup import setup_async_logging
from market_data_feeder import MarketDataFeeder
from message_source import CsvTradeSource
from transport import TcpTransport


DEFAULT_PATH = Path("data/all_trades_2020-03-01.csv")
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8001
LINE_LIMIT = 100_000
LOG_PATH = Path("logs/feeder.log")


def main() -> None:
    listener = setup_async_logging(LOG_PATH, logging.INFO)
    listener.start()

    logger = logging.getLogger("market_data_feeder")

    try:
        logger.info("feeder starting host=%s port=%d limit=%s", DEFAULT_HOST, DEFAULT_PORT, LINE_LIMIT)

        source = CsvTradeSource(DEFAULT_PATH)
        encoder = ProtobufOrderEncoder()
        transport = TcpTransport(DEFAULT_HOST, DEFAULT_PORT)
        feeder = MarketDataFeeder(source, encoder, transport)
        feeder.run(limit=LINE_LIMIT)

        logger.info("feeder stopped")
    finally:
        listener.stop()


if __name__ == "__main__":
    main()
```

This replaces `print(...)` style status output with background-file logging.

### 3. Change the decorator to record useful data

Move decorator code to:

```text
market_data_feeder/logger/decorators.py
```

The decorator should use `try/finally`:

```text
start timer
try:
    return original function result
finally:
    compute elapsed time
    log elapsed time or update metrics
```

Use `time.perf_counter_ns()` for latency timing.

Example:

```python
# market_data_feeder/logger/decorators.py

import functools
import logging
import time
from collections.abc import Callable
from typing import TypeVar, ParamSpec


P = ParamSpec("P")
R = TypeVar("R")


def log_latency(name: str, sample_rate: int = 1) -> Callable[[Callable[P, R]], Callable[P, R]]:
    if sample_rate <= 0:
        raise ValueError("sample_rate must be greater than 0")

    logger = logging.getLogger("market_data_feeder.latency")
    call_count = 0

    def decorator(func: Callable[P, R]) -> Callable[P, R]:
        nonlocal call_count

        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> R:
            nonlocal call_count

            start_ns = time.perf_counter_ns()
            try:
                return func(*args, **kwargs)
            finally:
                elapsed_ns = time.perf_counter_ns() - start_ns
                call_count += 1

                if call_count % sample_rate == 0:
                    logger.debug("latency name=%s elapsed_ns=%d", name, elapsed_ns)

        return wrapper

    return decorator
```

Usage:

```python
from logger.decorators import log_latency


class ProtobufOrderEncoder:
    @log_latency("protobuf_encode", sample_rate=1000)
    def encode(self, message):
        ...
```

This logs one timing sample every 1000 calls. That is much better than logging every packet.

### 4. Add sampling

Do not log every function call by default.

Use one of these policies:

- log every N calls
- log only if elapsed time is above a threshold
- log aggregate stats every N messages

For example:

```text
sample_rate=1000
```

means log one timing event every 1000 calls.

### 5. Add aggregate metrics

For feeder throughput, aggregate counters are more useful than individual log lines.

Track:

```text
message_count
encode_total_ns
send_total_ns
start_time_ns
```

At the end of the run, log:

```text
messages sent
total runtime
messages per second
average encode ns
average send ns
```

This gives a cleaner signal than a massive log file.

Example:

```python
# market_data_feeder/metrics.py

from dataclasses import dataclass
import time


@dataclass
class FeederMetrics:
    message_count: int = 0
    encode_total_ns: int = 0
    send_total_ns: int = 0
    start_ns: int = 0

    def start(self) -> None:
        self.start_ns = time.perf_counter_ns()

    def record_encode(self, elapsed_ns: int) -> None:
        self.encode_total_ns += elapsed_ns

    def record_send(self, elapsed_ns: int) -> None:
        self.send_total_ns += elapsed_ns
        self.message_count += 1

    def runtime_ns(self) -> int:
        return time.perf_counter_ns() - self.start_ns

    def messages_per_second(self) -> float:
        runtime_seconds = self.runtime_ns() / 1_000_000_000
        if runtime_seconds == 0:
            return 0.0
        return self.message_count / runtime_seconds

    def average_encode_ns(self) -> float:
        if self.message_count == 0:
            return 0.0
        return self.encode_total_ns / self.message_count

    def average_send_ns(self) -> float:
        if self.message_count == 0:
            return 0.0
        return self.send_total_ns / self.message_count
```

Then in `MarketDataFeeder.run`, the shape would be:

```python
import logging
import time
from itertools import islice
from typing import Optional

from metrics import FeederMetrics


class MarketDataFeeder:
    def __init__(self, source, encoder, transport):
        self.source = source
        self.encoder = encoder
        self.transport = transport
        self.logger = logging.getLogger("market_data_feeder")

    def run(self, limit: Optional[int] = None) -> None:
        metrics = FeederMetrics()
        metrics.start()

        messages = self.source.messages()
        if limit is not None:
            messages = islice(messages, limit)

        with self.transport as transport:
            for message in messages:
                encode_start_ns = time.perf_counter_ns()
                payload = self.encoder.encode(message)
                metrics.record_encode(time.perf_counter_ns() - encode_start_ns)

                send_start_ns = time.perf_counter_ns()
                transport.send(payload)
                metrics.record_send(time.perf_counter_ns() - send_start_ns)

        self.logger.info(
            "feeder_summary messages=%d msgs_per_sec=%.2f avg_encode_ns=%.2f avg_send_ns=%.2f",
            metrics.message_count,
            metrics.messages_per_second(),
            metrics.average_encode_ns(),
            metrics.average_send_ns(),
        )
```

This gives you useful throughput numbers without producing one log line per message.

## Suggested File Layout

```text
market_data_feeder/
  logger/
    logger.py              # current experiment, can be replaced or split
    setup.py               # async logging configuration
    decorators.py          # latency decorators
    LOW_LATENCY_LOGGING.md # this note
```

If `logger.py` grows too large, split it. A setup module and a decorator module have different responsibilities.

## Should We Use picologging?

Not yet.

`picologging` may be faster than the standard logging module, but it adds an external dependency and its Python 3.14 story may not be as smooth as the standard library.

For this project, the better learning order is:

1. built-in logging with direct file output
2. built-in logging with `QueueHandler` and `QueueListener`
3. sampling and aggregate metrics
4. benchmark the overhead
5. only then compare `picologging`

If the standard queue-based approach is not the bottleneck, adding another library is unnecessary.

## Important Tradeoffs

### Unbounded Queue

`SimpleQueue` avoids blocking the feeder, but memory can grow if logs are produced too quickly.

Mitigation:

- sample logs
- aggregate metrics
- avoid per-message logs

### Bounded Queue

A bounded queue limits memory, but then you need a policy when the queue is full:

- block the feeder
- drop the log
- count dropped logs

For low-latency systems, dropping logs is usually better than blocking the hot path.

### Text Logs

Text logs are easy to read but expensive to produce.

For Python, readable text logs are fine because the feeder is not the matching engine.

For C++, prefer structured or binary telemetry when you start optimizing seriously.

## Success Criteria

The first version is good enough if:

- the feeder can run with logging enabled
- logs are written by a background listener thread
- the hot loop does not write directly to disk
- per-message logging is avoided or sampled
- final throughput stats are logged at the end
- the implementation remains easy to understand

## Commands

From the shared Python project:

```bash
cd /Users/curtischang/Documents/C++/OrderBook/market_data_feeder
uv run python main.py
```

For a syntax check:

```bash
uv run python -m py_compile \
  main.py \
  encoder.py \
  market_data_feeder.py \
  message_source.py \
  order_input.py \
  transport.py \
  logger/logger.py \
  generated/order_pb2.py
```

For profiling:

```bash
uv run python -m cProfile -o feeder.prof main.py
uv run python -m pstats feeder.prof
```
