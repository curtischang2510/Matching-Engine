# The L3 local order book (market-by-order)

You want true L3 — individual order identity preserved, and time order within each price level (so you can later ask "where is my order in the queue"). That rules out the aggregate-only design. But it does **not** force you into soft-delete, and it gives you a cleaner answer to the linked-list cache worry than tombstones do.

## What L3 actually requires

1. **Find any order by id** in O(1) — for Execute / Cancel / Delete / Replace.
2. **Remove an order at an arbitrary position** in O(1) — cancels/deletes hit orders anywhere in a level, not just the front.
3. **Preserve time order within a level** — front = oldest, for L3 depth and queue position.
4. **Answer L1/L2 fast** — best bid/ask, aggregated depth per level.

Requirement 2 is the one that killed the deque-index idea (indices shift). The clean fix is a **container whose per-element handles stay stable** across other insertions/removals. `std::list` is exactly that.

## The key realization: `std::list` iterators are stable handles

`std::list::iterator` stays valid across insertions and across erasures *of other elements* — it's invalidated only when its own element is erased. So `order_id -> iterator` is a **stable handle** (unlike an index), and `list::erase(iterator)` is a **real O(1) removal** at any position.

That means you get L3 with O(1) find + O(1) arbitrary delete **without soft-delete at all** — no tombstones, no reclamation policy, no bloat. The whole problem soft-delete was trying to work around (O(1) middle removal in contiguous storage) just isn't there: the list gives you O(1) middle removal directly.

## The structure

```cpp
using OrderId = std::uint64_t;
using Price   = std::uint32_t;   // ticks
using Qty     = std::uint32_t;
enum class Side { BUY, SELL };

struct Order {
    OrderId id;
    Qty shares;                 // remaining
    // side/price are known from the Level this order lives in
};

struct Level {
    Price price;
    Qty totalQty = 0;           // aggregate, maintained incrementally for L2
    std::list<Order> orders;    // time order: front = oldest (queue head)
};

struct Locator {                // where an order lives
    Side side;
    Price price;
    std::list<Order>::iterator it;
};

class LocalOrderBook {
public:
    void apply(const AddOrder&);
    void apply(const OrderExecuted&);
    void apply(const OrderCancel&);
    void apply(const OrderDelete&);
    // void apply(const OrderReplace&);   // later

    // queries
    std::optional<Price> bestBid() const;   // bids_.rbegin()
    std::optional<Price> bestAsk() const;   // asks_.begin()
    // L2: iterate the maps; L3: iterate a Level's orders

private:
    std::map<Price, Level> bids_;                  // rbegin() = best (highest)
    std::map<Price, Level> asks_;                  // begin()  = best (lowest)
    std::unordered_map<OrderId, Locator> index_;   // O(1) find by id

    std::map<Price, Level>& sideMap(Side s) { return s == Side::BUY ? bids_ : asks_; }
};
```

Three layers, each with the right complexity: `unordered_map` for O(1) id lookup, `std::map<Price,Level>` for O(log n) price ordering (and O(1) best via begin/rbegin), `std::list` for O(1) time-ordered insert/erase within a level.

Why `std::map<Price,Level>` and not something flatter: `std::map` is node-based, so a `Level` never moves once inserted — the `std::list` inside it and every iterator into that list stay valid even as you add/remove *other* price levels. That stability is what makes the `index_` handles safe.

## The operations

```cpp
void LocalOrderBook::apply(const AddOrder& m) {
    auto& side = sideMap(m.side);
    Level& lvl = side.try_emplace(m.header /*price*/, Level{/*price*/}).first->second;
    auto it = lvl.orders.insert(lvl.orders.end(), Order{m.order_id, m.shares});  // append = newest
    lvl.totalQty += m.shares;
    index_[m.order_id] = Locator{m.side, /*price*/, it};
}

void LocalOrderBook::apply(const OrderExecuted& m) {
    reduce(m.order_id, m.executed_shares);   // shared helper below
}
void LocalOrderBook::apply(const OrderCancel& m) {
    reduce(m.order_id, m.canceled_shares);
}

void LocalOrderBook::apply(const OrderDelete& m) {
    auto found = index_.find(m.order_id);
    if (found == index_.end()) return;             // unknown id -> ignore (or log)
    Locator loc = found->second;
    Level& lvl = sideMap(loc.side).at(loc.price);
    lvl.totalQty -= loc.it->shares;                // remove full remaining
    lvl.orders.erase(loc.it);                      // O(1)
    index_.erase(found);
    if (lvl.orders.empty()) sideMap(loc.side).erase(loc.price);
}
```

Shared reduce helper (Execute and Cancel are the same operation — subtract shares, and if it hits zero the order is gone):

```cpp
void LocalOrderBook::reduce(OrderId id, Qty by) {
    auto found = index_.find(id);
    if (found == index_.end()) return;
    Locator loc = found->second;
    Level& lvl = sideMap(loc.side).at(loc.price);
    loc.it->shares -= by;
    lvl.totalQty  -= by;
    if (loc.it->shares == 0) {                     // executed/cancelled to zero -> implicit remove
        lvl.orders.erase(loc.it);
        index_.erase(found);
        if (lvl.orders.empty()) sideMap(loc.side).erase(loc.price);
    }
}
```

That "reduce to zero removes the order" is the ITCH rule from the wire-format notes — no separate Delete is sent when an Execute/Cancel empties an order. Write a test for exactly that case.

**Replace** (when you add it): the message carries old id, new id, new price, new shares — but *not* side or symbol (unchanged). So read `side`/`price`... no — read `side` from the **old order's Locator** before deleting it, then Delete(oldId) + Add(newId, thatSide, newPrice, newShares).

## Queries

- **L1**: `bestBid = bids_.rbegin()->first` (highest price), `bestAsk = asks_.begin()->first` (lowest). Spread = ask − bid. Both O(1). Guard for empty sides with `std::optional`.
- **L2**: walk `bids_` (reverse) / `asks_` (forward), emit `{price, level.totalQty}` — `totalQty` is already maintained, so no per-order summing.
- **L3**: walk a specific `Level`'s `orders` list — that's your market-by-order view and where queue position lives (your order's distance from `orders.begin()`).

## The cache concern — solved by pooling, not tombstones

Your original worry about linked lists is legit: `std::list` heap-allocates every node separately, so traversal chases scattered pointers (a cache miss per order). But the right fix is **not** soft-delete-in-a-deque — that trades the pointer-chase for tombstone bloat *and* you still touch dead cache lines skipping tombstones, so the locality win largely evaporates under heavy cancels.

The real fix, once correctness is locked and *if profiling says node allocation matters*:

- Replace `std::list<Order>` with an **intrusive doubly-linked list whose nodes live in a contiguous pool/arena** (a `std::vector<Node>` plus a free-list of reclaimed slots). Same O(1) insert/erase logic, same stable-handle property (the handle becomes a pool index or `Node*`), but the nodes sit in contiguous memory, so traversal is cache-friendly and there's one bump-allocation instead of a `new` per order.

This is the standard "fast limit order book" structure, and it's a natural fit with the pool/`CircularBuffer` work already in V2 — but it's a measured optimization, so build the `std::list` version first, prove it correct against the feed, then swap the container with the logic unchanged.

## Recommended path

1. **v1:** `std::list`-backed L3 exactly as above. Correct, real O(1) deletes, no soft-delete, stable iterator handles. Test each ITCH op + the reduce-to-zero rule + L1/L2/L3 queries.
2. **Later, measured:** swap `std::list` for a pool-backed intrusive list to fix node-allocation locality. Logic unchanged; only the container and the handle type change.

Soft-delete stays a known technique, but for an L3 book with heavy cancels it's the wrong trade — the list gives you O(1) arbitrary removal outright, and pooling gives you the locality you wanted, without ever accumulating dead entries.
