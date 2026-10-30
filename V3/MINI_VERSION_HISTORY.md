# V2 Mini Version History

This file tracks the smaller V2 milestones so the optimization work does not become one large, hard-to-understand change.

The current git history only has one explicit V2 commit:

- `7cbc3ea` - async low-latency logging prototype with a lock-free SPSC buffer and Python async logging

Some V2 work exists in the current working tree but is not represented as a separate historical commit yet. Treat the versions below as a learning/project ledger, not a strict git tag list.

## Version Table

| Version | Status | Commit / Reference | Main Goal | Main Changes | Verification |
| --- | --- | --- | --- | --- | --- |
| V2.0 | Baseline | Copied from V1 | Create a separate optimization playground | V2 starts from the V1 order book, protobuf networking path, tests, and commands. | Build should match the V1 behavior before optimization starts. |
| V2.1 | Current V2 worktree | `V2/CircularBuffer/`, `V2/OrderBook.*` | Replace simple FIFO storage with a resizable circular-buffer style queue | Introduces a custom `CircularBuffer` abstraction for order queues. This is the first step toward understanding memory layout, allocation behavior, and cache effects instead of only relying on `std::deque`. | Order book tests should still pass after replacing the queue internals. |
| V2.2 | Committed | `7cbc3ea` | Add low-latency logging without putting file I/O on the hot path | Adds C++ async logging with `AsyncLogger`, `LogEvent`, and `SPSCBuffer`. Adds Python logging practice using async queue-style logging/decorators. | Logger tests should validate queue invariants and dropped-message accounting. |
| V2.3 | In progress | Current staged V2 files | Move internal ticker representation from strings to `StockTicker` enum | Parse ticker strings once at the protobuf boundary, then route/log internally using `StockTicker`. Adds `StockTickerTests`, enum hashing, symbol ids, and cleaner logging data. | Build `OrderBook`, run `OrderBookTests`, `LoggerTests`, `StockTickerTests`, then run `ctest`. |
| V2.4 | Planned | None yet | Measure logging and add-order latency cleanly | Add reusable timing helper and compile-time logging switch so release builds can compare logging-on vs logging-off behavior. | Compare Release builds with `ORDERBOOK_ENABLE_LOGGING=ON` and `OFF`. |
| V2.5 | Planned | None yet | Improve routing data structures | Consider replacing `unordered_map<StockTicker, OrderBook>` with an array/vector indexed by stable ticker id once the ticker universe is fixed. | Benchmark routing overhead before and after. |

## Notes By Version

### V2.1 - Resizable Circular Buffer

Purpose:

- Learn how queue storage affects performance.
- Reduce dependence on `std::deque` internals.
- Keep the matching algorithm understandable before doing deeper hardware tuning.

Things to watch:

- Does the buffer preserve FIFO behavior exactly?
- What happens when the buffer grows?
- Are references or pointers invalidated after resize?
- Are tests covering wraparound behavior?

### V2.2 - Low-Latency Logging

Purpose:

- Keep the order book hot path from blocking on disk I/O.
- Push compact log events into a queue.
- Let a background consumer thread serialize events to disk.

Important learning point:

- This does not make logging free. It moves most of the expensive work away from the producer thread. The hot path still pays for event construction, queue push, atomics, cache traffic, and possible dropped-event accounting.

Things to watch:

- Queue full behavior should be explicit.
- Dropped log count should be visible during shutdown.
- The logger must shut down cleanly with RAII.
- Tests should prove the SPSC invariant: one producer, one consumer.

### V2.3 - StockTicker Enum

Purpose:

- Avoid passing ticker strings through the internal C++ hot path.
- Make logging cheaper by storing a small symbol id.
- Keep protobuf and Python unchanged for now.

Boundary rule:

- Strings enter from protobuf.
- `OrderProtoAdapter` converts the string into `StockTicker`.
- Unknown tickers are rejected before reaching `Exchange` / `OrderBook`.
- Internal code should use `StockTicker`, not `std::string`.

Things to watch:

- `toString(StockTicker)` is only for output/debug/protobuf conversion.
- `toSymbolId(StockTicker)` should remain stable if log files depend on it.
- If the ticker universe grows a lot, a large `switch` may become less attractive than generated lookup data or a perfect-hash approach.

## Future Optimization Ideas

- Add benchmarks that separate:
  - protobuf frame read
  - protobuf parse
  - ticker parse
  - exchange routing
  - order book matching
  - logging enqueue
- Add Release benchmarks with logging enabled and disabled.
- Use Instruments to compare flame graphs before and after each mini version.
- Consider array-based order book routing once ticker ids are stable.
- Consider fixed-capacity pools for order storage after correctness is locked down.
- Consider binary log output if text formatting becomes a major logger cost.

## Version Discipline

For each future mini version, record:

- What changed.
- Why it changed.
- Which files were touched.
- What correctness tests passed.
- What performance result changed, if measured.
- Whether the change made the code harder to understand.

The goal is not just to make the order book faster. The goal is to understand which change produced which behavior.
