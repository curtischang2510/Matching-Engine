# System wiring — exchange vs market maker

## The mental model that was wrong before

Earlier docs said "replay each CSV trade as an Add + Execute ITCH pair." That conflated the exchange's **input** with its **output**. The correct separation:

- **Order flow IN** (the CSV, for now) — trades/orders *arriving at* the exchange. This is raw input. Its exact values don't matter to the feed; it's just a source of activity.
- **Market data OUT** (ITCH over MoldUDP) — what the exchange *generates and publishes* after processing that flow. This is a product of the exchange's own logic, not a passthrough of the input.

The exchange sits in the middle transforming one into the other. The market maker only ever sees the output side — it has no idea the CSV exists.

```
order flow  --->  [ EXCHANGE: process, generate market data ]  --->  ITCH feed  --->  market maker
 (input)                                                              (output)
```

## The one hard requirement on the exchange's output

The market maker doesn't care whether the exchange does true price-time matching or just fabricates activity — it only needs the ITCH stream to be a **coherent order lifecycle**:

- Every `Order Executed` / `Cancel` / `Delete` must reference an order id that a prior `Add Order` created and that hasn't already been fully removed.
- Sequence numbers strictly increase by message count per packet (so gap detection works).

That's the real constraint, and it's *lighter than a matching engine*. A generator that adds orders, tracks which ids are live, and later executes/deletes only live ids is enough to exercise the entire C++ consumer — including the order-id index — with well-formed data. Whether the exchange also does real matching (crossing bids against asks) is an optional later upgrade, not a blocker.

**Scope decision to make now:** simple lifecycle generator (recommended first) vs. real matching engine. If real matching, note the matching code (`OrderBook.cpp` / `Exchange.cpp`, currently sitting in the C++ `MarketMaker` target) is on the wrong side of the boundary — the market maker must *never* match, it only applies deltas. That C++ matching code either moves conceptually to "the exchange" or gets deleted from the market maker.

## Component status

### Exchange (Python, `V3/exchange/`)

| Piece | File | Status |
|---|---|---|
| Order-flow ingest | `message_source.py` (`CsvTradeSource`) | done (input source) |
| ITCH message model | `ITCH/model.py` | done |
| ITCH encoder | `ITCH/encoder.py` | done, tested |
| MoldUDP packet builder | `MoldUDP/mold_udp.py` | done |
| Multicast publisher | `MoldUDP/feed_publisher.py` | done |
| Sent-message history | `MoldUDP/history.py` | done |
| Retransmission responder | `MoldUDP/retransmission.py` | done, tested |
| **Market-data generator** (flow → coherent ITCH lifecycle) | `market_data_generator.py` (NEW) | **missing — the real gap** |
| **`main.py` wiring** | `main.py` | **still the old protobuf/TCP path — not wired to any of the above** |

### Market maker (C++, `V3/` root) — all still to build

| Piece | Status |
|---|---|
| Multicast receiver (join group, `recvfrom` on the feed socket) | to build |
| MoldUDP packet parser + sequence tracking + gap detection | to build |
| Retransmission client (send request *from the feed socket* on gap; handle out-of-order replies) | to build |
| ITCH message parser (bytes → message structs) | to build |
| Local order book, **order-id-keyed, apply-only** (never matches) | to build |
| `OrderBook.cpp` / `Exchange.cpp` (matching engine, inherited from V2) | wrong side — remove or relocate (see scope decision) |

## What "writing it all up" means on the Python side

Two concrete pieces:

### 1. `market_data_generator.py` (new)

Owns the counters and lifecycle state the exchange is responsible for:

- assigns order reference numbers (monotonic) and match numbers — this is the "who assigns the order id" answer
- maps symbol → `stock_locate` (must match the C++ `StockTicker::toSymbolId`, so the two sides agree)
- generates the `float → ticks` conversion for price (the single edge where a float exists)
- **tracks live order ids** so it only emits Execute/Cancel/Delete against orders that exist — this is what makes the output a coherent lifecycle
- yields `list[ItchMessage]` per unit of input activity

The input (`OrderInput` from `CsvTradeSource`) is a *trigger* for activity, not a template to copy verbatim onto the wire.

### 2. `main.py` rewrite

Replace the protobuf/TCP body with the publish pipeline (the `SERVER_SETUP.md` sketch, updated for the generator):

```
set up logging
history   = SentMessageHistory()
responder = RetransmissionResponder(REQUEST_HOST, REQUEST_PORT, history); responder.start()
publisher = MulticastFeedPublisher(MCAST_GROUP, MCAST_PORT, MCAST_INTERFACE)
generator = MarketDataGenerator()

sequence = 1
for activity in CsvTradeSource(...).messages():
    for message in generator.on_input(activity):      # coherent ITCH lifecycle
        encoded = encode_itch_message(message)
        publisher.send(MoldUdpPacketBuilder(sequence).add(encoded).build())
        history.record(sequence, encoded)
        sequence += 1
finally: publisher.close(); responder.stop(); responder.join(); listener.stop()
```

(Batching several messages per packet is an optimization; one-message-per-packet is fine to start and keeps sequence math trivial.)

The old `exchange.py`, `encoder.py` (protobuf), `transport.py` (`TcpTransport`) are no longer on the market-data path. Keep them only if you still want the protobuf/TCP order-entry channel later; otherwise they're dead.
