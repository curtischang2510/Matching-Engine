# Simple ITCH replay — parse a line, encode, fire

Supersedes `GENERATOR_AND_MAIN.md`'s `market_data_generator.py`. The assumption now: the input is **already a stream of ITCH messages** in text form, one per line. The exchange keeps **no state** — it just parses each line into the matching message, encodes it, wraps it in a MoldUDP packet, and multicasts it.

```
text line  ->  ItchMessage  ->  bytes  ->  MoldUDP packet  ->  multicast
```

No order-id tracking, no matching, no coherence checks. If the input file is incoherent (a Delete for an order that was never Added), that's the input's problem — garbage in, garbage out. The Python side is a dumb pipe.

## Input line format

Comma-separated. First field is the message type letter; the rest are the message's fields **in the same order as the dataclass**, already as integers (price is in ticks, timestamp is nanoseconds since midnight — this is "already ITCH", so no conversion happens here).

```
A,stock_locate,timestamp_ns,order_id,side,shares,stock,price
E,stock_locate,timestamp_ns,order_id,executed_shares,match_number
X,stock_locate,timestamp_ns,order_id,canceled_shares
D,stock_locate,timestamp_ns,order_id
```

`side` is `B` or `S` (the `Side` enum values). Everything else is an integer except `stock`, which is a plain symbol string.

Example input file (`feed.itch`):

```
A,2,1000,1,B,500,BTCUSDT,853150000
A,2,1100,2,S,300,BTCUSDT,853200000
E,2,1200,1,500,1
D,2,1300,2
```

That's: add buy order 1 (500 @ 85315.0000), add sell order 2 (300 @ 85320.0000), fully execute order 1 (match #1), delete order 2.

## `itch_line_source.py` (new, at `exchange/` root)

```python
from typing import Iterator

from ITCH.model import (
    Side,
    AddOrderMessage,
    OrderExecutedMessage,
    OrderCancelMessage,
    OrderDeleteMessage,
    ItchMessage,
)


def parse_itch_line(line: str) -> ItchMessage:
    parts = line.strip().split(",")
    message_type = parts[0]

    # Tuple unpacking raises ValueError on the wrong field count -> loud failure,
    # which is what you want for a malformed feed line.
    if message_type == "A":
        _, locate, ts, order_id, side, shares, stock, price = parts
        return AddOrderMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            side=Side(side),                 # "B" -> Side.BUY, "S" -> Side.SELL
            shares=int(shares),
            stock=stock,
            price=int(price),                # already in ticks
        )
    if message_type == "E":
        _, locate, ts, order_id, executed, match = parts
        return OrderExecutedMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            executed_shares=int(executed),
            match_number=int(match),
        )
    if message_type == "X":
        _, locate, ts, order_id, canceled = parts
        return OrderCancelMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            canceled_shares=int(canceled),
        )
    if message_type == "D":
        _, locate, ts, order_id = parts
        return OrderDeleteMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
        )
    raise ValueError(f"unknown ITCH message type: {message_type!r}")


def messages_from_file(path) -> Iterator[ItchMessage]:
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):    # allow blanks + comments
                continue
            yield parse_itch_line(line)
```

The dataclasses here are transient — created, encoded, dropped. That's still "no data structures" in the sense you meant: nothing is *kept*. (You could parse straight to bytes and skip the dataclass, but reusing the tested `model` + `encode_itch_message` costs nothing and catches field-count bugs.)

## `main.py` (rewrite)

```python
import logging
from pathlib import Path

from ITCH.encoder import encode_itch_message
from MoldUDP.mold_udp import MoldUdpPacketBuilder
from MoldUDP.history import SentMessageHistory
from MoldUDP.retransmission import RetransmissionResponder
from MoldUDP.feed_publisher import MulticastFeedPublisher
from itch_line_source import messages_from_file
from logger.setup import set_async_logging

ITCH_INPUT_PATH = Path("feed.itch")
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
    responder.start()
    logger.info("responder listening on %s:%d", REQUEST_HOST, REQUEST_PORT)

    publisher = MulticastFeedPublisher(MCAST_GROUP, MCAST_PORT, MCAST_INTERFACE)

    sequence = 1                          # one message per packet -> +1 each time
    try:
        for message in messages_from_file(ITCH_INPUT_PATH):
            encoded = encode_itch_message(message)
            publisher.send(MoldUdpPacketBuilder(sequence).add(encoded).build())
            history.record(sequence, encoded)
            sequence += 1

        logger.info("feed complete, next sequence=%d", sequence)
        input("feed done - serving retransmissions; press Enter to shut down\n")
    finally:
        publisher.close()
        responder.stop()
        responder.join(timeout=2)
        listener.stop()


if __name__ == "__main__":
    main()
```

That's the whole exchange now: one line → one message → one packet. Sequence increments by one because each packet carries exactly one message, so `history.record(sequence, ...)` and the retransmission math stay trivial.

## Smoke test

Terminal 1 (from `exchange/` root, so the `ITCH.` / `MoldUDP.` package imports resolve):

```bash
cd V3/exchange
uv run --project . python main.py
```

Terminal 2 — join the group and watch packets:

```python
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("", 9002))
mreq = socket.inet_aton("239.1.1.1") + socket.inet_aton("127.0.0.1")
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
for _ in range(4):
    data, _ = sock.recvfrom(65535)
    print(len(data), "bytes", data[10:11])   # [10:11] = the ITCH type byte
```

For the 4-line example you'd expect: `46 b'A'`, `46 b'A'`, `41 b'E'`, `29 b'D'` (10-byte MoldUDP header + one length-prefixed ITCH message: 2 + 34/34/29/17).
