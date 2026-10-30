# What's left on the C++ side — networking + the order-id book

Two big pieces: (1) the UDP receiver + application loop, and (2) redesigning the book to apply updates by order id. The second has a design decision worth thinking through — your soft-delete idea is a good instinct but the exact mechanism you described has a bug, and there may be a simpler path entirely.

---

# 1. The UDP socket + application layer

## Socket setup (boilerplate — fill-in is minimal)

`Networking/MulticastReceiver.h`, wrapping the fd in your existing `SocketHandle`:

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <stdexcept>
#include "SocketHandle.h"

class MulticastReceiver {
public:
    MulticastReceiver(const char* group, std::uint16_t port, const char* iface) {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) throw std::runtime_error("socket() failed");
        sock_ = SocketHandle{fd};

        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);   // bind the port on all interfaces
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");

        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = ::inet_addr(group);   // 239.1.1.1
        mreq.imr_interface.s_addr = ::inet_addr(iface);   // 127.0.0.1
        if (::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            throw std::runtime_error("join group failed");
    }

    // blocking; returns number of bytes in one datagram
    std::size_t receive(std::uint8_t* buf, std::size_t cap) {
        ssize_t n = ::recvfrom(sock_.fd(), buf, cap, 0, nullptr, nullptr);
        if (n < 0) throw std::runtime_error("recvfrom() failed");
        return static_cast<std::size_t>(n);
    }

    // retransmission request, sent FROM this same socket (src port = feed port)
    void sendRequest(const std::uint8_t* data, std::size_t size,
                     const char* host, std::uint16_t port) {
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port = htons(port);          // 9001
        dst.sin_addr.s_addr = ::inet_addr(host);
        ::sendto(sock_.fd(), data, size, 0,
                 reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    }

private:
    SocketHandle sock_;
};
```

Two rules baked in: `sendto` (never `connect()` — that would filter out the multicast feed), and the request goes out this same socket so the reply lands back here.

## One small addition to `ByteReader`

For zero-copy message slicing in the loop, add a "view n bytes and advance" method:

```cpp
const std::uint8_t* take(std::size_t n) { need(n); auto* p = data_ + pos_; pos_ += n; return p; }
```

## Application layer — SKELETON (this is the part to fill in)

This is the interesting bit: MoldUDP framing, sequence/gap handling, dispatch to the book.

```cpp
void run(MulticastReceiver& rx, LocalOrderBook& book) {
    std::uint64_t expected = 1;             // matches the exchange's start
    std::uint8_t buf[2048];

    while (true) {
        std::size_t n = rx.receive(buf, sizeof(buf));   // one datagram

        ByteReader r{buf, n};
        const std::uint64_t seq   = r.u64();            // MoldUDP header
        const std::uint16_t count = r.u16();

        // ---- GAP HANDLING (fill in) ------------------------------------
        if (seq > expected) {
            // missed [expected, seq). Send a request for them.
            //   auto req = buildRequest(expected, seq - expected);  // 10 bytes, >QH
            //   rx.sendRequest(req.data(), req.size(), "127.0.0.1", 9001);
            // Decide: buffer this ahead-of-time packet, or drop and rely on
            // the retransmit to redeliver in order. (Buffering is nicer; dropping
            // is simpler for v1.)
            // TODO
        } else if (seq + count <= expected) {
            // wholly old (dup / late retransmit already applied) -> skip
            continue;
        }
        // NOTE: seq < expected but seq+count > expected means a retransmit that
        // partially overlaps — you must skip the messages already applied.

        // ---- MESSAGE DISPATCH ------------------------------------------
        for (std::uint16_t i = 0; i < count; ++i) {
            const std::uint16_t len = r.u16();
            const std::uint8_t* msg = r.take(len);
            const std::uint64_t msgSeq = seq + i;

            if (msgSeq < expected) continue;            // already have this one

            auto parsed = parseItch(msg, len);
            if (!parsed) {
                // TODO: malformed / unknown type — log and continue
                continue;
            }
            std::visit([&](auto&& m){ book.apply(m); }, *parsed);   // apply by type
            expected = msgSeq + 1;
        }
    }
}
```

What's stubbed for you: the gap request/buffering policy, the malformed handling, and `book.apply(...)` (section 2). The `std::visit` + a `book.apply(AddOrder)`, `apply(OrderExecuted)`, etc. overload set is the clean way to dispatch the variant.

---

# 2. The order-id book — evaluating your soft-delete idea

## First, the reframe that changes everything: the market maker does not match

Your V2 `OrderBook` stores bare quantities in a FIFO per level because a *matching engine* consumes the front of the queue to decide fills. **The market maker never matches** — it only applies deltas the exchange already decided. That changes what the book needs, and it's worth pinning down before picking a data structure.

What each ITCH message needs to do:

| Message | Operation | Needs |
|---|---|---|
| Add | insert order (id, side, price, shares) | find nothing; just insert |
| Execute | reduce order's shares by id; remove if 0 | **find order by id** |
| Cancel | reduce order's shares by id | **find order by id** |
| Delete | remove order by id | **find order by id** |
| Replace | delete old id, add new id | **find order by id** |

Three of five operations are "find a specific order **by id**, at an arbitrary position, and change it." That's the whole problem to solve.

## Your proposal, and the bug in it

> soft-delete instead of removing from the deque; hashmap of order_id → index in the deque for O(1) random access.

The **soft-delete instinct is legitimately good** — logical deletion + lazy reclamation is a real technique, and avoiding a naive `std::list` (pointer-chasing, a cache-miss per node) is the right thing to worry about.

But **keying the hashmap by *index* into the deque doesn't work**, because indices aren't stable:

- A `std::deque` index is relative to the current front. The moment you `pop_front` (which you'll do when the front order is executed/consumed), **every remaining order's index shifts by one**, and your entire `order_id → index` map is silently wrong. Off-by-N corruption, no crash.
- Same problem when you eventually compact away the tombstones ("poll from the deque later") — compaction moves elements, so all indices change and the map must be rebuilt (O(n)).

So the index map breaks under exactly the two operations you need (front consumption + compaction). Indices are the wrong handle.

There's also a second issue independent of indices: **cancels dominate in real markets** (often >90% of orders are cancelled, not filled). Soft-delete means tombstones pile up fast, so a price level's deque bloats with dead entries, and your matching-side traversal wastes time skipping them. The reclamation story ("poll later") isn't a detail — it's load-bearing, and it's the hard part.

## Better options, in order of how much I'd recommend them

### Option A (recommended for v1): no per-level queue at all

Since the market maker only needs L1/L2 (best bid/ask, aggregated depth) and to apply deltas by id, you may not need a per-level ordered container *at all*:

```cpp
struct RestingOrder { Side side; std::uint32_t price; std::uint32_t shares; };

std::unordered_map<std::uint64_t, RestingOrder> orders_;   // by order id
std::map<std::uint32_t, std::uint64_t> bidQty_;            // price -> total qty
std::map<std::uint32_t, std::uint64_t> askQty_;            // price -> total qty
```

- **Add**: `orders_[id] = {...}`; `bidQty_[price] += shares`. O(1) / O(log n).
- **Execute/Cancel**: look up `orders_[id]`, reduce shares and the level total; if the order hits 0, erase it from `orders_`; if the level hits 0, erase the price key.
- **Delete**: subtract the order's full remaining from its level, erase from `orders_`.
- **L1**: best bid = `bidQty_.rbegin()` (highest price), best ask = `askQty_.begin()` (lowest). **L2**: iterate the map.

No deque, no linked list, no soft-delete, no tombstones — every op is O(1) or O(log n), and it's the least code. This works because **you dropped the FIFO-within-a-level requirement, which only a matching engine needs.**

### Option B (if you later want true L3 / queue position)

A market maker eventually cares about *where its own order sits in the queue* at a level (fill-probability estimation). That needs per-level time order preserved. The industry-standard structure for that with O(1) arbitrary delete is an **intrusive doubly-linked list per price level, with a `order_id → node*` hashmap** — delete is O(1) (unlink the node), lookup is O(1). The cache concern about linked lists is solved by **allocating the nodes from a contiguous object pool/arena**, not scattered `new`s. That's the "fast limit order book" design real systems use.

### Option C (your soft-delete, fixed)

If you want to keep the deque + soft-delete shape: key the map by a **stable handle, not an index**. `std::deque` pointers/references survive `push_back` and `pop_front` *of other elements* (unlike `std::vector`), so `order_id → RestingOrder*` (a pointer into the deque) stays valid where an index wouldn't. You still need a reclamation policy (lazily drop tombstones as they reach the front; full compaction rebuilds the map), and you still pay the tombstone-bloat cost. It's more moving parts than Option A for the same L1/L2 result.

## My recommendation

Start with **Option A**. It's the simplest thing that correctly applies every ITCH message by id and answers L1/L2, and it drops the FIFO machinery you don't need because you're not matching. Reach for **Option B** only when you actually build the strategy layer and need queue position — and at that point the pooled intrusive list, not soft-delete, is the move. Keep soft-delete (Option C) in your back pocket as a known technique, but it solves a problem (O(1) middle-delete in contiguous storage) that Option A sidesteps entirely.
