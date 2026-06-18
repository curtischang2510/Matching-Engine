# Order Book Learning Project

This project exists to learn C++ and market microstructure together. The goal is
not to rush toward a finished matching engine. The goal is to build an order
book slowly enough that each design choice teaches something useful about C++,
finance, performance, and clean software engineering.

## Project Philosophy

- Learn by implementing, measuring, and explaining decisions.
- Prefer clear, correct C++ before clever C++.
- Avoid copying complete order book solutions.
- Treat the assistant as a teacher, reviewer, and debugging partner, not as a
  code generator that finishes the project for you.
- When a concept is new, first write down the idea in plain English.
- When code becomes messy, stop and improve the design before adding more
  features.
- When performance matters, measure before guessing.
- Every optimization should answer: what bottleneck did this improve, and how
  did we verify it?

## How The Assistant Should Help

The assistant should:

- Explain concepts before providing code.
- Ask guiding questions when the learning value is higher than giving the
  answer directly.
- Review your C++ for correctness, readability, ownership, naming, API design,
  and unnecessary complexity.
- Suggest cleaner designs when appropriate, such as replacing raw strings or
  magic numbers with `enum class`, value types, or named constants.
- Help design tests and benchmarks.
- Help interpret compiler errors, sanitizer output, profiler results, and
  performance measurements.
- Avoid spoon-feeding full order book implementations unless you explicitly ask
  for a reference solution after attempting it yourself.

## Learning Goals

- Understand how limit order books represent liquidity.
- Understand L1 and L2 market data.
- Understand price-time priority.
- Practice C++ classes, constructors, member functions, access control, enums,
  containers, references, const-correctness, and basic RAII.
- Learn how to write code that is readable and testable.
- Learn how to profile C++ code on a MacBook and improve real bottlenecks.
- Build toward quantitative finance knowledge through market structure,
  latency, data structures, and measurement.

## Important Market Concepts

### L1 Order Book

L1 is the top of the book:

- Best bid price.
- Quantity available at the best bid.
- Best ask price.
- Quantity available at the best ask.
- Spread, which is `best ask - best bid`.

L1 answers: what is the best immediately visible price to buy or sell?

### L2 Order Book

L2 is aggregated depth by price level:

- Multiple bid price levels.
- Multiple ask price levels.
- Total quantity available at each price level.
- Usually no individual order identities are shown in public L2 data.

L2 answers: how much visible liquidity exists at different prices?

### Price-Time Priority

For this project, Phase 1 uses FIFO priority at each price level:

- Better prices execute first.
- At the same price, older orders execute before newer orders.
- Bids are prioritized from highest price to lowest price.
- Asks are prioritized from lowest price to highest price.

## Phase 1 Scope

Phase 1 is a simple, single-threaded order book. The focus is correctness,
clarity, and learning basic C++ design.

### Phase 1 Non-Goals

- No multi-threading.
- No lock-free data structures.
- No exchange protocol parsing.
- No persistence.
- No networking.
- No advanced hardware optimization.
- No custom allocator at the beginning.
- No attempt to handle every real exchange edge case.

## Phase 1 Checklist

### C++ Foundations

- [ ] Understand what a class is.
- [ ] Understand public vs private members.
- [ ] Understand constructors.
- [ ] Understand member functions.
- [ ] Understand `const` member functions.
- [ ] Understand references and when to avoid unnecessary copies.
- [ ] Understand `enum class`.
- [ ] Understand basic standard containers such as `std::vector`, `std::deque`,
      `std::map`, and `std::unordered_map`.
- [ ] Understand how to split code into `.hpp` and `.cpp` files.
- [ ] Understand how to compile with warnings enabled.

### Domain Model

- [ ] Define what an order is.
- [ ] Define what an order id is.
- [ ] Define what side means: buy or sell.
- [ ] Define what order type means for Phase 1.
- [ ] Define what price means.
- [ ] Define what quantity means.
- [ ] Decide whether price and quantity should be integers instead of floating
      point values.
- [ ] Define what a trade/fill is.
- [ ] Define what a price level is.
- [ ] Define what the full order book owns.

### Basic Order Book Behavior

- [ ] Add a buy limit order.
- [ ] Add a sell limit order.
- [ ] Match an incoming buy order against resting asks.
- [ ] Match an incoming sell order against resting bids.
- [ ] Partially fill an order.
- [ ] Fully fill an order.
- [ ] Leave the unfilled remainder resting in the book when appropriate.
- [ ] Preserve FIFO order within the same price level.
- [ ] Return generated trades from an order entry operation.
- [ ] Reject or handle invalid orders such as zero quantity.

### L1 Behavior

- [ ] Report best bid.
- [ ] Report best ask.
- [ ] Report best bid quantity.
- [ ] Report best ask quantity.
- [ ] Report spread when both sides exist.
- [ ] Handle an empty bid side.
- [ ] Handle an empty ask side.
- [ ] Handle a completely empty book.

### L2 Behavior

- [ ] Report aggregated bid depth by price level.
- [ ] Report aggregated ask depth by price level.
- [ ] Return levels in the correct order.
- [ ] Limit output to the top `N` levels when requested.
- [ ] Update aggregated depth after fills.
- [ ] Remove empty price levels.

### Testing

- [ ] Test adding one buy order.
- [ ] Test adding one sell order.
- [ ] Test no match when prices do not cross.
- [ ] Test full match.
- [ ] Test partial match.
- [ ] Test FIFO matching at the same price.
- [ ] Test best bid and best ask updates.
- [ ] Test L2 aggregation.
- [ ] Test empty book behavior.
- [ ] Test invalid order handling.

### Clean Code Review Points

- [ ] Use expressive names.
- [ ] Keep each class responsible for one clear thing.
- [ ] Prefer `enum class Side { Buy, Sell };` over raw strings like `"buy"`.
- [ ] Prefer named types or aliases for ids, prices, and quantities when useful.
- [ ] Avoid exposing internal containers directly.
- [ ] Mark read-only member functions as `const`.
- [ ] Avoid global mutable state.
- [ ] Keep matching logic easy to read before optimizing it.
- [ ] Add comments only where the intent is not obvious from the code.
- [ ] Write small tests before adding larger features.

## Suggested Phase 1 Data Structures

Start simple:

- Use one ordered container for bid price levels.
- Use one ordered container for ask price levels.
- Use a FIFO container at each price level.
- Use integer prices and quantities.

Possible C++ standard library choices:

- `std::map` for price levels.
- `std::deque` for FIFO orders at a price level.
- `std::vector` for returning trades or L2 snapshots.

For bids, remember that the best price is the highest price. For asks, the best
price is the lowest price. A clean design should make this easy to reason about.

## Suggested Domain Types

These are not final answers, but they are concepts you should consider:

- `OrderId`
- `Price`
- `Quantity`
- `Side`
- `Order`
- `Trade`
- `PriceLevel`
- `OrderBook`
- `BookLevel`
- `L1Snapshot`
- `L2Snapshot`

Design question to answer before coding: which of these should be classes,
which should be structs, and which should just be type aliases at first?

## Example C++ Class Unrelated To Order Books

This example is intentionally unrelated to finance. Study the shape of the
class: private data, public behavior, constructor, `const` methods, and a small
enum.

```cpp
#include <string>
#include <utility>

enum class LampMode {
    Off,
    Low,
    High
};

class DeskLamp {
public:
    DeskLamp(std::string name, int max_brightness)
        : name_(std::move(name)),
          max_brightness_(max_brightness),
          mode_(LampMode::Off) {}

    void turn_off() {
        mode_ = LampMode::Off;
    }

    void set_low() {
        mode_ = LampMode::Low;
    }

    void set_high() {
        mode_ = LampMode::High;
    }

    bool is_on() const {
        return mode_ != LampMode::Off;
    }

    int current_brightness() const {
        if (mode_ == LampMode::Off) {
            return 0;
        }

        if (mode_ == LampMode::Low) {
            return max_brightness_ / 2;
        }

        return max_brightness_;
    }

    const std::string& name() const {
        return name_;
    }

private:
    std::string name_;
    int max_brightness_;
    LampMode mode_;
};
```

Things to notice:

- The data members are private.
- Users of the class interact through member functions.
- The constructor creates a valid object.
- `is_on`, `current_brightness`, and `name` are marked `const` because they do
  not modify the object.
- `LampMode` is an `enum class`, which is safer and clearer than strings.
- The class protects its internal state instead of letting outside code modify
  fields directly.

Questions to ask when designing your first order book classes:

- What state does this object own?
- What operations should outside code be allowed to perform?
- What invariants must always remain true?
- Which member functions should be `const`?
- Which details should stay private?

## Hardware And Profiling Goals

Do not optimize hardware behavior in Phase 1 before correctness is stable. Once
the simple implementation works, use measurement to guide improvements.

MacBook-focused topics to learn later:

- Compiler optimization levels such as `-O0`, `-O2`, and `-O3`.
- Debug vs release builds.
- Apple Clang warning flags.
- AddressSanitizer and UndefinedBehaviorSanitizer.
- Instruments for profiling CPU time.
- Cache locality.
- Branch prediction.
- Heap allocation costs.
- Value layout and object size.
- Avoiding unnecessary copies.
- Benchmarking with stable, repeatable input data.

Useful questions for every performance change:

- What exact workload are we measuring?
- What metric changed?
- Did correctness remain the same?
- Did the change make the code harder to understand?
- Is the complexity worth it at this phase?

## Possible Project Milestones

- [ ] Milestone 1: Build the smallest class example yourself.
- [ ] Milestone 2: Define the core order book vocabulary in comments or notes.
- [ ] Milestone 3: Create basic order, trade, and side types.
- [ ] Milestone 4: Add simple order insertion without matching.
- [ ] Milestone 5: Add matching for one incoming order.
- [ ] Milestone 6: Add L1 snapshot.
- [ ] Milestone 7: Add L2 snapshot.
- [ ] Milestone 8: Add unit tests.
- [ ] Milestone 9: Add simple benchmarks.
- [ ] Milestone 10: Profile and improve one measured bottleneck.

## Personal Rules For This Project

- Before writing a class, describe its responsibility in one sentence.
- Before adding a data member, explain why the class should own it.
- Before optimizing, write down the measurement.
- Before asking for a full solution, attempt a small version first.
- After each feature, write what you learned.
