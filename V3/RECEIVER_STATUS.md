# C++ market maker receiver — status and what's left

## What works today (verified end-to-end)

Python `feed.itch` → ITCH encode → MoldUDP → multicast → C++ receive → parse → L3 book → L1, proven correct against a 6-message feed (add / execute-to-empty / partial-cancel / delete all reflected correctly in best bid/ask).

| Piece | File | State |
|---|---|---|
| Big-endian byte reader | `Networking/ByteReader.h` | done (`u8/u16/u32/u64/bytes/take`) |
| ITCH structs + variant | `ITCH/itch.h` | done (A/E/X/D) |
| ITCH parser | `ITCH/ItchParser.h` | done, `inline`, `nullopt` on bad/unknown |
| L3 order book (std::list) | `OrderBook/LocalOrderBook.{h,cpp}` | done: apply Add/Exec/Cancel/Delete, L1/L2, reduce-to-zero removal |
| UDP multicast socket | `Networking/MulticastReceiver.h` | done: join group, receive, sendRequest |
| Receive loop | `main.cpp` | works for the **in-order** case; gap handling is a stub |
| Build | `CMakeLists.txt` | `MarketMaker` target = the receiver |

## Things to add

### 1. Reliable in-order delivery + retransmission (the big one)

This is more than "send a request" — the current loop doesn't actually handle gaps correctly yet. Two problems to fix:

**Problem A — `expected` start value.** The feed starts at sequence **1**, but `main.cpp` now sets `expected = 0`. So the first packet (seq 1) trips `seq > expected` and logs a false gap. Fix: start `expected = 1`, or better, initialize it from the first packet seen (`if (firstPacket) expected = seq;`) so the receiver doesn't assume a start value.

**Problem B — ahead-of-sequence packets are mis-applied.** Right now a message is applied whenever `msgSeq >= expected`. That means if packet seq 5 arrives while `expected == 1`, its messages get applied immediately and `expected` jumps to 6 — then when the retransmitted 1–4 arrive, they're `< expected` and get **dropped**. So gaps currently corrupt the book silently. Correct behavior needs a **reorder buffer** and strict in-order application:

```
state:
  expected                       // next seq to apply
  std::map<uint64_t, Message> reorder;   // messages seen ahead of `expected`

for each message m at msgSeq in the packet:
    if      (msgSeq <  expected)  continue;              // dup / already applied
    else if (msgSeq == expected){ apply(m); ++expected;  // in order
                                  drainReorder(); }      // apply any now-contiguous buffered msgs
    else                          reorder[msgSeq] = m;   // ahead -> buffer, do NOT apply

drainReorder():
    while reorder.contains(expected){ apply(reorder[expected]); reorder.erase(expected); ++expected; }

// after the packet: if there's still a gap (expected < first buffered seq),
// make sure a retransmission request for [expected, firstBuffered) is outstanding.
```

**The actual retry/timeout logic** (your "retry logic") sits on top of that:

- On detecting a gap, send `buildRequest(expected, missingCount)` via `rx.sendRequest(...)` to `127.0.0.1:9001`, and **record** `{range, sentTime, attempts}`.
- **UDP can lose the request too**, so you need a timeout: if the gap isn't filled within `T` (a few ms), resend (`attempts++`), up to `maxAttempts`. Add exponential-ish backoff if you want.
- After `maxAttempts` with no fill, you're out of options with the current design — log and either give up on those seqs or trigger a full resync (snapshot recovery, which we don't have — out of scope for now).
- **The blocking-recv trap:** `recvfrom` blocks, so while waiting for a packet you can't fire a retry timer. Fix by giving the socket a receive timeout (`setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, ...)`) so `receive()` returns periodically even with no traffic; on each wakeup, check whether any outstanding request has timed out and resend. (Alternative: non-blocking socket + `poll`/`select`.)

Minimal version to start: reorder buffer + in-order apply + one request per gap + a fixed-timeout single retry. Layer backoff/maxAttempts on later.

### 2. Reject duplicates from retransmission cleanly

Once retransmission works, the same message can arrive twice (original late + retransmit). The `msgSeq < expected` skip already handles this — but make sure a retransmitted **reply packet** (which arrives on the same socket, looking like any feed packet) flows through the exact same path. It does, by design; just confirm with a test that drops a packet and checks the book still converges.

### 3. Smaller gaps

- **`expected` init** — see Problem A (do this first, it's one line).
- **Order Replace (`U`)** — not parsed yet. Adds two order ids in one message (cancel old + add new). Wire it in `ItchParser` and `LocalOrderBook::apply` (delete old id, add new; carry side from the old order).
- **Multi-symbol** — the book currently ignores `stock_locate` and keeps one book. For >1 symbol, route by locate (`unordered_map<uint16_t, LocalOrderBook>` or an array).
- **Malformed / short packets** — handled (parser returns `nullopt`, loop `break`s), but currently just logged; decide if a bad packet should also trigger a resync.

### 4. Later, not tonight

- **Remove the debug prints** (`std::unitbuf` + per-packet L1) from the hot path — observation only, not for a latency build.
- **Pooled intrusive list** — swap `std::list` for the arena + intrusive DLL once correctness is locked and profiling justifies it. Logic unchanged.
- **Unit tests** — `ItchParserTests` (golden bytes from the Python encoder) and `LocalOrderBookTests` (apply sequences, assert L1/counts + the reduce-to-zero rule). The live run covers the happy path; tests lock it against regressions and cover the gap/dup cases.
- **Order entry** — the dashed arrow in the diagram; sending orders back to the exchange, out of scope for now.

## Suggested next session order

1. Fix `expected` init (1 line, removes the false-gap noise).
2. Reorder buffer + strict in-order apply (Problem B) — this makes gaps *safe* even before retransmission exists (missed messages just wait).
3. Retransmission request + `SO_RCVTIMEO` + single retry.
4. A test that deliberately drops a packet and asserts the book still converges.
