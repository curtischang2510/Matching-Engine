# OrderBook

A learning project exploring market microstructure and low-latency C++, built up as an **exchange + market-maker simulation**.

![OrderBook architecture](./V3/diagrams/exchange_market_maker_overview.svg)

## V3 — exchange & market maker

Two processes that talk the way a real venue and a subscriber do:

- **Exchange** (`V3/exchange/`, Python) — publishes an order-by-order market-data feed and serves gap-fill requests.
- **Market maker** (`V3/`, C++) — subscribes, parses the feed, and reconstructs the live order book.

The exchange encodes book events as **ITCH** — NASDAQ's binary, order-by-order feed format (Add / Executed / Cancel / Delete) — and ships them over **MoldUDP**, a lightweight sequenced protocol over UDP multicast. The market maker joins the multicast group and rebuilds an **L3 (market-by-order)** book keyed by order id. Dropped packets show up as sequence gaps and are recovered with unicast retransmission requests — sent and received on the same socket the live feed arrives on, so a retransmit is indistinguishable from a normal packet.

`V1/` and `V2/` are earlier exchange (matching-engine) versions: `V1` a readable baseline, `V2` a performance playground.

## Future

- Replace the L3 book's `std::list` with an **intrusive doubly-linked list** over a **contiguous memory pool**. O(1) alloc/free, no per-node allocation, cache-friendly traversal.
- Migrate the exchange from **Python** to **Rust**
