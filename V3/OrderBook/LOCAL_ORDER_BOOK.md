# LocalOrderBook — how it works

The book has to do two different kinds of lookup, and that tension explains the whole design:

1. **By price** — "what's the best bid?", "how much is resting at 100?" → needs prices *sorted*.
2. **By order id** — an `Execute`/`Cancel`/`Delete` message only gives you an **order id**, and you must find that exact order to shrink or remove it → needs *O(1) lookup from an id*.

No single container does both, so the book keeps three, and the `Locator` is the glue between them.

## The three structures

```cpp
// order id -> where that order physically lives
std::unordered_map<std::uint64_t, Locator> index_;

// price -> everything resting at that price, sorted by price
std::map<std::uint32_t, Level> bids_;   // best bid = highest price = rbegin()
std::map<std::uint32_t, Level> asks_;   // best ask = lowest  price = begin()
```

| Container | Key | Value | Why this container |
|---|---|---|---|
| `index_` | order id (`uint64`) | `Locator` (where the order is) | hash map → O(1) find-by-id |
| `bids_` / `asks_` | price in ticks (`uint32`) | `Level` (all orders at that price) | ordered map → O(1) best price via `begin`/`rbegin`, sorted L2 walk |

```cpp
struct Order  { std::uint64_t id;    std::uint32_t shares; };            // one resting order (shares = remaining)
struct Level  { std::uint32_t price; std::uint64_t totalQty;             // running sum of shares at this price (for fast L2)
                std::list<Order> orders; };                             // orders in time order: front = oldest
struct Locator{ Side side; std::uint32_t price;                         // which map + which key
                std::list<Order>::iterator it; };                       // exact node inside that Level's list
```

## What the Locator actually does

An `Execute`/`Cancel`/`Delete` hands you **only an order id**. To act on that order you need to reach the actual `Order` object sitting inside some `Level`'s list — and to update the `Level` (its `totalQty`, and maybe erase it if it empties) you need to know *which* level. The `Locator` is the **full address** that answers all of that in one hop:

- `side` → tells you which map to look in (`BUY` → `bids_`, `SELL` → `asks_`).
- `price` → the key into that map, giving you the `Level`.
- `it` → a `std::list` iterator pointing straight at the `Order` node inside that level's list.

So `index_[id]` → `Locator` → (`side`, `price`) locates the `Level`, and `it` locates the `Order` within it. Without the `Locator` you'd only have an id and no way to find the order except scanning every level — O(n). With it, every by-id operation is a hash lookup plus direct pointers.

Why the iterator is safe to store: `std::list` iterators stay valid when *other* elements are inserted or erased — only erasing the element itself invalidates it. So `index_` can hold these iterators long-term, and erasing an order from a list is O(1) with no effect on any other order's `Locator`.

## The shape, drawn

A book after adding: order 1 (BUY 500 @ 100), order 2 (SELL 300 @ 101), order 3 (BUY 200 @ 100).

```
        index_  (unordered_map<order id, Locator>)
        ┌─────────────────────────────────────────────┐
        │ 1 → Locator{ side=BUY,  price=100, it ─┐ }   │
        │ 2 → Locator{ side=SELL, price=101, it ─┼─┐ } │
        │ 3 → Locator{ side=BUY,  price=100, it ─┼─┼─┐}│
        └────────────────────────────────────────┼─┼─┼┘
                                                  │ │ │  (each `it` points AT one Order node below)
  bids_ (map<price, Level>)                       │ │ │
  ┌──────────────────────────────────────────┐   │ │ │
  │ 100 → Level{ price=100, totalQty=700,     │   │ │ │
  │              orders = [ Order{id=1,sh=500} ,  Order{id=3,sh=200} ] }
  │                          ▲                        ▲
  └──────────────────────────┼────────────────────────┼──────────────┘
                             └ index_[1].it            └ index_[3].it
  asks_ (map<price, Level>)
  ┌──────────────────────────────────────────┐
  │ 101 → Level{ price=101, totalQty=300,     │       │
  │              orders = [ Order{id=2,sh=300} ] }     │
  │                          ▲                         │
  └──────────────────────────┼─────────────────────────┘
                             └ index_[2].it
```

Two things to notice:
- `totalQty` (700 at price 100) is just the sum of the `shares` in that level's list, kept up to date on every change so L2 depth never has to re-add the list.
- The list preserves **time priority** — order 1 is ahead of order 3 at price 100 because it arrived first (front = oldest). That ordering is L3's whole point.

## Operation flow

**Add** (`apply(AddOrder)`)
1. `sideMap(side)[price]` — grab (or create) the `Level` for this price.
2. `orders.insert(end, {id, shares})` — append at the back (newest → time priority preserved). This returns the iterator.
3. `totalQty += shares`.
4. `index_[id] = { side, price, iterator }` — record the address so future messages can find it.

**Execute / Cancel** (`apply(OrderExecuted)` / `apply(OrderCancel)` → both call `reduce(id, by)`)
1. `index_.find(id)` → `Locator` (ignore if unknown — never-seen id).
2. From `Locator`: `sideMap(side).find(price)` → the `Level`.
3. `it->shares -= by` and `totalQty -= by` (clamped so it can't go negative).
4. If `it->shares == 0`: the order is fully gone → `orders.erase(it)`, `index_.erase(id)`, and if the level is now empty, erase the price key too. (This is the ITCH rule: an execute/cancel that hits zero *implicitly* removes the order — no separate Delete is sent.)

**Delete** (`apply(OrderDelete)`)
Same address lookup, but remove the whole remaining order: `totalQty -= it->shares`, `orders.erase(it)`, `index_.erase(id)`, drop the level if empty.

**Best prices** (`top()`)
- Best bid = `bids_.rbegin()` — highest price (a `std::map` is sorted ascending, so the last element is the highest).
- Best ask = `asks_.begin()` — lowest price.
Both O(1); `totalQty` gives the size at that level for free.

## Suggested key/value comments for the header

These make the intent obvious at the declaration site:

```cpp
struct Order
{
    std::uint64_t id;
    std::uint32_t shares;   // remaining shares (shrinks on execute/cancel)
};

struct Level
{
    std::uint32_t price;
    std::uint64_t totalQty;      // sum of shares in `orders` (maintained incrementally for L2)
    std::list<Order> orders;     // time priority: front = oldest, back = newest
};

struct Locator                   // the full "address" of one order
{
    Side side;                   // which map: BUY -> bids_, SELL -> asks_
    std::uint32_t price;         // the key into that map -> the Level
    std::list<Order>::iterator it;   // the exact Order node inside that Level's list
};

// key: order id      value: where the order lives (side + price + list node)
std::unordered_map<std::uint64_t, Locator> index_;

// key: price (ticks) value: all orders resting at that price
std::map<std::uint32_t, Level> bids_;   // best bid = rbegin() (highest price)
std::map<std::uint32_t, Level> asks_;   // best ask = begin()  (lowest price)
```

## One-line mental model

`index_` answers *"where is order N?"* with a `Locator`; the `Locator` walks you into the right `bids_`/`asks_` level and hands you the exact list node — so every add/execute/cancel/delete is a couple of pointer hops instead of a scan, while `bids_`/`asks_` keep prices sorted so best-bid/ask and L2 depth stay cheap.
