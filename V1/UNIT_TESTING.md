# Unit Testing In C++

This project should add unit tests before networking. Tests give you a fast way
to check that the order book still behaves correctly after each design change.

## What A Unit Test Is

A unit test is a small program that checks one behavior.

For this order book, useful unit tests are things like:

- Add one bid and confirm it rests in the book.
- Add one ask and confirm it rests in the book.
- Add a bid and ask that do not cross and confirm no fill happens.
- Add a bid and ask that cross and confirm a fill happens.
- Confirm FIFO behavior at the same price.
- Confirm partial fills leave the correct remaining quantity.

The test should be small enough that when it fails, you know what behavior broke.

## Recommended First Approach

Start with a simple test executable using `assert`.

Do not start with a full framework like GoogleTest yet. Frameworks are useful,
but your first goal is to understand the testing loop:

```text
write test -> compile -> run test -> see pass/fail -> fix code
```

Once the basic workflow feels natural, you can move to GoogleTest or Catch2.

## Suggested File Layout

Add a `tests` directory:

```text
OrderBook/
  CMakeLists.txt
  OrderBook.cpp
  OrderBook.h
  Exchange.cpp
  Exchange.h
  Trade.h
  main.cpp
  tests/
    OrderBookTests.cpp
```

The test file is just another C++ program. It has its own `main()` function.

## Example Test File

Create `tests/OrderBookTests.cpp` with a shape like this:

```cpp
#include "../OrderBook.h"

#include <cassert>
#include <iostream>

void test_non_crossing_orders_do_not_fill()
{
    OrderBook book;

    Trade bid{"AAPL", OrderType::BID, 100.0, 10};
    std::vector<Fill> bidFills = book.addOrder(bid);
    assert(bidFills.empty());

    Trade ask{"AAPL", OrderType::ASK, 101.0, 5};
    std::vector<Fill> askFills = book.addOrder(ask);
    assert(askFills.empty());
}

void test_crossing_order_fills()
{
    OrderBook book;

    Trade ask{"AAPL", OrderType::ASK, 100.0, 5};
    book.addOrder(ask);

    Trade bid{"AAPL", OrderType::BID, 101.0, 5};
    std::vector<Fill> fills = book.addOrder(bid);

    assert(fills.size() == 1);
    assert(fills[0].price == 100.0);
    assert(fills[0].qty == 5);
}

int main()
{
    test_non_crossing_orders_do_not_fill();
    test_crossing_order_fills();

    std::cout << "All OrderBook tests passed\n";
    return 0;
}
```

This is intentionally simple. `assert(condition)` crashes the program if the
condition is false. If the program exits normally, the test passed.

## Important C++ Detail

Your production executable and test executable should not both compile the same
`main.cpp`.

The normal app has one `main()` function.
The test executable has its own separate `main()` function.

That is why the test executable should compile:

```text
tests/OrderBookTests.cpp
OrderBook.cpp
Exchange.cpp, only if the tests need Exchange
```

It should not compile:

```text
main.cpp
```

## Example CMake Setup

Your current `CMakeLists.txt` creates only one executable. You can extend it like
this:

```cmake
cmake_minimum_required(VERSION 4.2)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OrderBook
        main.cpp
        OrderBook.cpp
        OrderBook.h
        Exchange.cpp
        Exchange.h
        Trade.h)

enable_testing()

add_executable(OrderBookTests
        tests/OrderBookTests.cpp
        OrderBook.cpp
        OrderBook.h
        Trade.h)

add_test(NAME OrderBookTests COMMAND OrderBookTests)
```

`enable_testing()` turns on CTest support.

`add_test(...)` tells CMake how to run your test executable.

## How To Run Tests

From the project root, configure/build as usual. In CLion, you can usually reload
CMake after editing `CMakeLists.txt`.

From a terminal, if `cmake` is available:

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
```

In your current shell, plain `cmake` may not be on `PATH`. The CLion bundled
CMake path has worked before:

```bash
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake -S . -B cmake-build-debug
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake --build cmake-build-debug
/Users/curtischang/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/ctest --test-dir cmake-build-debug --output-on-failure
```

If everything passes, you should see output showing that `OrderBookTests` passed.

## What To Test First

Start with these tests:

- [ ] Empty book plus one bid returns no fills.
- [ ] Empty book plus one ask returns no fills.
- [ ] Bid below best ask does not fill.
- [ ] Ask above best bid does not fill.
- [ ] Bid equal to ask price fills.
- [ ] Bid above ask price fills at resting ask price.
- [ ] Ask below bid price fills at resting bid price.
- [ ] Partial fill leaves remaining resting quantity.
- [ ] FIFO at the same price level.

Do not write twenty tests at once. Write one test, run it, understand the
failure, then move to the next.

## When To Move Beyond `assert`

Move to GoogleTest or Catch2 when:

- You want clearer failure messages.
- You want many test files.
- You want setup/teardown helpers.
- You want parameterized tests.
- You want better IDE integration.

For now, `assert` is enough to learn the core loop.

## Testing Philosophy For This Project

- Test behavior, not private implementation details.
- Use small inputs where the expected result is obvious.
- Prefer readable tests over clever tests.
- When a bug is found, write a test that fails before fixing it.
- Keep networking out of the first unit tests.
- Keep performance benchmarks separate from correctness tests.

