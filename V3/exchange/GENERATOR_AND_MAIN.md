# `market_data_generator.py` and `main.py`

Reference implementation for the two missing pieces. The generator turns incoming order flow into a **coherent ITCH lifecycle**; `main.py` wires it to the publisher, history, and retransmission responder you've already built.

The one principle to keep straight: the incoming `OrderInput` is a **trigger for activity**, not a template copied onto the wire. The generator owns the order ids, decides what events to emit, and guarantees every Execute/Delete references an order that is actually live.

---

## `market_data_generator.py` (new, at `exchange/` root)

```python
import time

from ITCH.model import (
    AddOrderMessage,
    OrderExecutedMessage,
    ItchMessage,
    Side,
)
from order_input import OrderInput

# MUST match V3/StockTicker/StockTicker.h -> toSymbolId(). If these drift, the
# market maker labels every order with the wrong symbol.
SYMBOL_TO_LOCATE = {
    "AAPL": 0,
    "VOO": 1,
    "BTCUSDT": 2,
    "XBTUSD": 3,
    "BTC-PERPETUAL": 4,
}
LOCATE_UNKNOWN = 5

PRICE_SCALE = 10_000            # Price(4): the single float -> int-ticks edge
NS_PER_DAY = 86_400_000_000_000
RETIRE_EVERY = 3               # fully execute the oldest live order every Nth input


def nanoseconds_since_midnight() -> int:
    return time.time_ns() % NS_PER_DAY


def to_ticks(price: float) -> int:
    return round(price * PRICE_SCALE)


class MarketDataGenerator:
    """Order flow in -> a coherent stream of ITCH messages out.

    Exchange-owned state lives here: order ids, match numbers, and the set of
    currently-live orders. This is deliberately NOT a matching engine; it is the
    smallest thing that emits a lifecycle the market maker can rebuild a book
    from. Swap the policy in on_input() for real matching later if you want.
    """

    def __init__(self):
        self._next_order_id = 1
        self._next_match_number = 1
        self._live: dict[int, dict] = {}     # order_id -> {stock_locate, remaining}
        self._input_count = 0

    def on_input(self, order: OrderInput) -> list[ItchMessage]:
        timestamp = nanoseconds_since_midnight()
        locate = SYMBOL_TO_LOCATE.get(order.symbol, LOCATE_UNKNOWN)
        messages: list[ItchMessage] = []

        # 1) a new order joins the book -> Add Order
        order_id = self._next_order_id
        self._next_order_id += 1
        messages.append(
            AddOrderMessage(
                stock_locate=locate,
                timestamp_ns=timestamp,
                order_id=order_id,
                side=Side.BUY if order.side == "BID" else Side.SELL,
                shares=order.quantity,
                stock=order.symbol,
                price=to_ticks(order.price),
            )
        )
        self._live[order_id] = {"stock_locate": locate, "remaining": order.quantity}

        # 2) periodically retire the oldest live order so the book churns and the
        #    consumer's Execute/removal path gets exercised. Coherent by
        #    construction: we only ever retire an id that is currently live.
        self._input_count += 1
        if self._input_count % RETIRE_EVERY == 0 and self._live:
            messages.append(self._execute_oldest(timestamp))

        return messages

    def _execute_oldest(self, timestamp: int) -> OrderExecutedMessage:
        # dict preserves insertion order, so the first item is the oldest.
        order_id, info = next(iter(self._live.items()))
        del self._live[order_id]                       # fully executed -> no longer live
        match_number = self._next_match_number
        self._next_match_number += 1
        return OrderExecutedMessage(
            stock_locate=info["stock_locate"],
            timestamp_ns=timestamp,
            order_id=order_id,
            executed_shares=info["remaining"],          # full fill
            match_number=match_number,
        )
```

Decisions baked in, worth knowing so you can change them deliberately:

- **No aggressor/resting side flip.** The earlier docs flipped the side because they misread a trade *print* as an execution. Here the input is an *order arriving*, so a `BID` input becomes an `Add` of side `BUY` directly. Simpler and correct for this framing.
- **The generation policy is a placeholder.** "Add every input, fully execute the oldest every 3rd" is the minimum coherent lifecycle — it exercises Add + Execute and keeps the live set bounded. It is not realistic matching. Making it smarter (partial executes, cancels, deletes, actual price crossing) only changes `on_input`; nothing downstream cares.
- **`shares` must fit `>I` (4 bytes).** The generator passes `order.quantity` straight through; if your input scaling can exceed ~4.29e9 it will raise at encode time. That's the input-data question you deferred — it surfaces here, not in this file's logic.
- **`stock` is truncated to 8 bytes by the encoder** — `"BTC-PERPETUAL"` goes out as `"BTC-PERP"`. The market maker should key identity off `stock_locate`, not the truncated string.

---

## `main.py` (rewrite — replaces the protobuf/TCP body)

```python
import logging
from itertools import islice
from pathlib import Path

from ITCH.encoder import encode_itch_message
from MoldUDP.mold_udp import MoldUdpPacketBuilder
from MoldUDP.history import SentMessageHistory
from MoldUDP.retransmission import RetransmissionResponder
from MoldUDP.feed_publisher import MulticastFeedPublisher
from market_data_generator import MarketDataGenerator
from message_source import CsvTradeSource
from logger.setup import set_async_logging

CSV_PATH = Path("../../order_flow/data/all_trades_2020-03-01.csv")
LINE_LIMIT = 100_000
LOG_PATH = Path("logs/exchange.log")

MCAST_GROUP = "239.1.1.1"
MCAST_PORT = 9002
MCAST_INTERFACE = "127.0.0.1"
REQUEST_HOST = "127.0.0.1"
REQUEST_PORT = 9001


def main():
    listener = set_async_logging(LOG_PATH, logging.DEBUG)
    listener.start()
    logger = logging.getLogger("exchange")

    history = SentMessageHistory()
    responder = RetransmissionResponder(REQUEST_HOST, REQUEST_PORT, history)
    responder.start()                       # spawn the listener thread; returns at once
    logger.info("responder listening on %s:%d", REQUEST_HOST, REQUEST_PORT)

    publisher = MulticastFeedPublisher(MCAST_GROUP, MCAST_PORT, MCAST_INTERFACE)
    generator = MarketDataGenerator()
    source = CsvTradeSource(CSV_PATH)

    sequence = 1                            # MoldUDP MESSAGE sequence number
    try:
        for order in islice(source.messages(), LINE_LIMIT):
            messages = generator.on_input(order)
            if not messages:
                continue

            encoded = [encode_itch_message(m) for m in messages]

            packet = MoldUdpPacketBuilder(sequence)   # one packet per input batch
            for message_bytes in encoded:
                packet.add(message_bytes)
            publisher.send(packet.build())

            for offset, message_bytes in enumerate(encoded):
                history.record(sequence + offset, message_bytes)   # key by message seq
            sequence += len(encoded)                  # advance by MESSAGE count

        logger.info("feed complete, next sequence=%d", sequence)
        input("feed done - serving retransmissions; press Enter to shut down\n")
    finally:
        publisher.close()
        responder.stop()                    # flag + close socket -> unblocks recvfrom
        responder.join(timeout=2)           # deterministic thread shutdown
        listener.stop()


if __name__ == "__main__":
    main()
```

Wiring notes:

- **`responder.start()` before the feed loop** — a subscriber could gap on the very first packets, so the request socket must already be bound and listening.
- **Sequence advances by message count, not packet count.** A packet with an Add + an Execute carries 2 messages, so `sequence += 2`, and each message is recorded under its own sequence (`sequence + offset`). This is what makes a retransmission request for a mid-packet message answerable.
- **One packet per input batch** keeps the mapping obvious. Batching more messages per packet is a throughput optimization for later.
- **`input(...)` parks the main thread** after the finite CSV ends, so the responder keeps serving retransmissions until you press Enter. A real exchange never reaches that line. If you run non-interactively, replace it with something that doesn't read stdin (e.g. `responder.join()` or a sleep).
- **`finally` order:** publisher first (nothing more goes out), then stop+join the responder, logger last (so shutdown lines still get written).
- The old `exchange.py`, `encoder.py` (protobuf), and `TcpTransport` are no longer on this path.

---

## Smoke test (no C++ yet)

Terminal 1 — run the exchange from the `exchange/` root (needed for the `ITCH.` / `MoldUDP.` package imports to resolve):

```bash
cd V3/exchange
uv run --project . python main.py
```

Terminal 2 — join the group and confirm packets arrive (from `MULTICAST_AND_RETRANSMISSION.md`):

```python
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("", 9002))
mreq = socket.inet_aton("239.1.1.1") + socket.inet_aton("127.0.0.1")
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
for _ in range(10):
    data, addr = sock.recvfrom(65535)
    print(len(data), "bytes")     # 46 for Add-only (10+2+34), 77 when a packet also carries an Execute
```

A steady mix of `46` and `77` byte packets means generator → encoder → packetizer → multicast all agree. Then request one back from the same socket with `build_request(1, 1)` to `127.0.0.1:9001` to confirm the retransmission path.
