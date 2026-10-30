# Building the C++ ITCH + MoldUDP receiver — where to start

Build **bottom-up**, and test each layer against hand-built bytes before adding the next. The order:

```
1. byte readers        (big-endian helpers)          ─┐ pure logic,
2. ITCH message structs (in-memory representation)     │ unit-testable
3. ITCH parser         (bytes -> struct)               │ with no sockets
4. MoldUDP parser      (packet -> messages + seq)     ─┘
5. networking          (the one UDP socket)           ─┐ needs the exchange
6. sequence / gap tracking                            ─┘ running
```

Networking comes **last** on purpose: it's the hardest thing to unit-test, and by the time you plug a socket in, parsing is already proven. This mirrors how the Python side went (encoder + tests before wiring the publisher).

---

## The one rule that saves you days: parse fields manually, never cast a struct over the buffer

The tempting move in C++ is to `#pragma pack(1)` a struct matching the wire and `reinterpret_cast<AddOrder*>(buffer)`. **Do not.** It will produce garbage, and here's exactly why:

1. **Endianness.** The wire is big-endian; Apple Silicon is little-endian. A cast reads the raw bytes in host order, so every multi-byte field (order id, price, timestamp) comes out byte-swapped.
2. **The fused header doesn't map to fields anyway.** Your header is a type byte + one 8-byte word holding `(stock_locate << 48) | timestamp_ns`. There's no struct layout that splits that back into a 16-bit and a 48-bit field.
3. **Alignment / strict aliasing.** Packed structs + casting a `char*` to a struct pointer is undefined behavior, and misaligned loads are a real hazard.

So the **in-memory struct is a different thing from the wire layout** — it holds natural host types, and you fill it by reading the wire field-by-field with explicit big-endian helpers. This is the same approach already in `Networking/ProtobufFrameReader.cpp` (`parseBigEndianLength`) — you're extending that pattern.

---

## Step 1 — big-endian byte reader

A small cursor that walks a buffer and reads big-endian integers, bounds-checked. Put it somewhere shared, e.g. `Networking/ByteReader.h`.

```cpp
#include <cstddef>
#include <cstdint>
#include <stdexcept>

class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    std::uint8_t u8()  { need(1); return data_[pos_++]; }

    std::uint16_t u16() {
        need(2);
        std::uint16_t v = (std::uint16_t(data_[pos_]) << 8) | data_[pos_ + 1];
        pos_ += 2; return v;
    }

    std::uint32_t u32() {
        need(4);
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | data_[pos_ + i];
        pos_ += 4; return v;
    }

    std::uint64_t u64() {
        need(8);
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | data_[pos_ + i];
        pos_ += 8; return v;
    }

    void bytes(char* out, std::size_t n) { need(n); std::memcpy(out, data_ + pos_, n); pos_ += n; }
    std::size_t remaining() const { return size_ - pos_; }

private:
    void need(std::size_t n) const {
        if (pos_ + n > size_) throw std::out_of_range("ByteReader: buffer underrun");
    }
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
};
```

That's the whole endianness story in one place. Everything above it is just calls to `u16/u32/u64`.

---

## Step 2 — ITCH message structs (grow `ITCH/itch.h`)

Replace the empty `struct Itch{}` stub. Plain structs, host-native types, tied together with `std::variant` (the C++ analogue of the Python union). This is a *parsed* representation — nothing here matches wire byte-order or sizes exactly.

```cpp
#include <array>
#include <cstdint>
#include <variant>

enum class Side { Buy, Sell };   // parsed from 'B' / 'S'

struct ItchHeader {
    std::uint16_t stock_locate;  // unpacked from the fused word's high 16 bits
    std::uint64_t timestamp_ns;  // low 48 bits
};

struct AddOrder {
    ItchHeader header;
    std::uint64_t order_id;
    Side side;
    std::uint32_t shares;
    std::array<char, 8> stock;   // 8-byte space-padded symbol
    std::uint32_t price;         // ticks (1/10000)
};

struct OrderExecuted {
    ItchHeader header;
    std::uint64_t order_id;
    std::uint32_t executed_shares;
    std::uint64_t match_number;
};

struct OrderCancel {
    ItchHeader header;
    std::uint64_t order_id;
    std::uint32_t canceled_shares;
};

struct OrderDelete {
    ItchHeader header;
    std::uint64_t order_id;
};

using ItchMessage = std::variant<AddOrder, OrderExecuted, OrderCancel, OrderDelete>;
```

Match these field widths against the wire (below) — they're the sizes the Python encoder produces.

### Wire layout (must match the Python `ITCH/encoder.py`)

```
Common header (9 bytes):
  type          1 byte   'A' 'E' 'X' 'D'
  fused word    8 bytes  big-endian uint64 = (stock_locate << 48) | timestamp_ns
                         → stock_locate = word >> 48;  timestamp_ns = word & 0xFFFFFFFFFFFF

A  (34): header + order_id(8) + side(1) + shares(4) + stock(8) + price(4)
E  (29): header + order_id(8) + executed_shares(4) + match_number(8)
X  (21): header + order_id(8) + canceled_shares(4)
D  (17): header + order_id(8)
```

Reading the 8-byte fused word and shifting is the payoff we designed for: no side ever touches an awkward 6-byte integer.

---

## Step 3 — ITCH parser (`ITCH/ItchParser.h/.cpp`)

One message's bytes → `ItchMessage`. Read the type byte, unpack the fused header, `switch`, read the rest.

```cpp
std::optional<ItchMessage> parseItch(const std::uint8_t* data, std::size_t size) {
    ByteReader r(data, size);
    const char type = static_cast<char>(r.u8());

    const std::uint64_t word = r.u64();
    ItchHeader h{ static_cast<std::uint16_t>(word >> 48),
                  word & 0xFFFFFFFFFFFFULL };

    switch (type) {
        case 'A': {
            AddOrder m{h};
            m.order_id = r.u64();
            m.side     = (r.u8() == 'B') ? Side::Buy : Side::Sell;
            m.shares   = r.u32();
            r.bytes(m.stock.data(), 8);
            m.price    = r.u32();
            return m;
        }
        // 'E', 'X', 'D' the same way
        default:
            return std::nullopt;   // unknown type -> caller decides
    }
}
```

Returning `std::optional` (or a small result type) keeps "malformed / unknown" explicit instead of throwing across the hot path. The `ByteReader` still throws on underrun; catch that at this boundary and turn it into `nullopt`.

---

## Step 4 — MoldUDP parser (`MoldUDP/MoldUdpParser.h/.cpp`)

A whole datagram → the starting sequence + each contained message. It does *not* apply anything — just frames.

```
sequence     8 bytes  BE  (sequence of the FIRST message in this packet)
count        2 bytes  BE  (0 = heartbeat)
then count times:
  length     2 bytes  BE
  message    <length> bytes   → hand to parseItch
```

Shape: read `sequence` and `count`, then loop `count` times slicing `length`-prefixed chunks. Return `{sequence, std::vector<ItchMessage>}` (or, to avoid copies, a vector of `{seq, span}` pairs the caller parses). A `count == 0` packet is a valid heartbeat — return an empty message list, not an error.

---

## Step 5 — networking: the ONE UDP socket

Per the interface diagram, the market maker has a single socket that receives the feed, sends retransmission requests, and receives the replies. Wrap the fd in the existing `Networking/SocketHandle` (RAII — it finally gets used).

Setup (same `setsockopt` calls as the Python receiver, since they're OS-level):

```cpp
int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
SocketHandle sock{fd};
int yes = 1;
::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_port = htons(9002);
addr.sin_addr.s_addr = htonl(INADDR_ANY);
::bind(fd, (sockaddr*)&addr, sizeof(addr));

ip_mreq mreq{};
mreq.imr_multiaddr.s_addr = ::inet_addr("239.1.1.1");   // group
mreq.imr_interface.s_addr = ::inet_addr("127.0.0.1");   // loopback interface
::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```

Receive loop: `::recvfrom(fd, buf, sizeof(buf), 0, ...)` → MoldUDP parser → ITCH parser → (later) the book.

Two C++ gotchas:

- **`SocketHandle::sendAll` uses `::send`, which is TCP-oriented** (stream, partial-write loop). For UDP a datagram is all-or-nothing — use `::sendto` directly for the retransmission request, or add a `sendTo(data, size, addr)` method to `SocketHandle`. Don't loop like `sendAll` does.
- **Never `connect()` this socket.** UDP `connect()` installs a peer filter and your multicast feed would stop arriving. Always `sendto()` for the request.

The retransmission request is the 10-byte `sequence(8) + count(2)` you defined in `RETRANSMISSION_REQUEST.md`, sent from this same socket to `127.0.0.1:9001`.

---

## Step 6 — sequence tracking / gap detection (stub for now)

Sits between the MoldUDP parser and the book. Keep one `std::uint64_t expected` (start at 1, matching the exchange). Per packet header sequence `s` with `count` messages:

- `s == expected`: apply all, `expected += count`.
- `s > expected`: gap `[expected, s)` — send a retransmission request for it; buffer or drop the ahead-of-time packet.
- `s < expected`: already seen (a late retransmit or dup) — apply only the still-missing ones, ignore the rest.

Leave this as a stub until parsing works end-to-end — it's the last thing, and it's where the single-socket design pays off (retransmit replies flow into the same loop, no special case).

---

## Testing: cross-language golden bytes

The strongest test, because both encoders are hand-rolled with no shared codegen: make the Python side emit a known message, capture the exact bytes, and assert your C++ parser reproduces the fields.

- Quick version: `uv run python -c "from ITCH.encoder import encode_itch_message; from ITCH.model import *; print(encode_itch_message(AddOrderMessage(2,1000,1,Side.BUY,500,'BTCUSDT',853150000)).hex())"`, paste the hex into a C++ test as a byte array, parse it, assert `order_id==1`, `price==853150000`, `stock=="BTCUSDT "`, etc.
- Do this for one of each type. If the two sides ever drift, this test breaks loudly.

Before that, unit-test each layer with hand-built byte arrays where you know the expected fields.

---

## File layout & first commit

```
V3/
  ITCH/
    itch.h            → the structs + variant + Side  (replace the stub)
    ItchParser.h/.cpp → parseItch
  MoldUDP/
    MoldUdpParser.h/.cpp
  Networking/
    ByteReader.h            → big-endian reader
    MulticastReceiver.h/.cpp → socket setup + recv loop (uses SocketHandle)
  tests/
    ItchParserTests.cpp
    MoldUdpParserTests.cpp
```

Add the new `.cpp` files to the `MarketMaker` target and give the tests their own executables in `CMakeLists.txt`, matching the existing `OrderBookTests` / `LoggerTests` pattern (plain `assert()` + a `main()`).

**Recommended first commit:** `ByteReader.h` + the ITCH structs in `itch.h` + `ItchParser` + `ItchParserTests` with hand-built bytes for all four types. No networking, no MoldUDP yet — smallest thing that's independently verifiable, exactly like the Python side started with the encoder + its test.
