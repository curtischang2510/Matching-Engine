cmake_minimum_required(VERSION 4.2)
project(MarketMaker)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ORDERBOOK_ENABLE_LOGGING "Enable async order book logging" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)

add_executable(MarketMaker
        main.cpp
        OrderBook/LocalOrderBook.cpp
        OrderBook/LocalOrderBook.h
        ITCH/itch.h
        ITCH/ItchParser.h
        Networking/ByteReader.h
        Networking/SocketHandle.h
        Networking/MulticastReceiver.h
        Networking/MoldUdpRequest.h)

target_include_directories(MarketMaker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()

add_executable(OrderBookTests
        tests/OrderBookTests.cpp
        OrderBook.cpp
        OrderBook.h
        Trade.h
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h
        ITCH/itch.h)

add_test(NAME OrderBookTests COMMAND OrderBookTests)

add_executable(LoggerTests
        tests/LoggerTests.cpp
        Logger/AsyncLogger.cpp
        Logger/AsyncLogger.h
        Logger/LogEvent.h
        Logger/SPSCBuffer.h
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h
        ITCH/ItchParser.h)

target_include_directories(LoggerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME LoggerTests COMMAND LoggerTests)

add_executable(StockTickerTests
        tests/StockTickerTests.cpp
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h)

add_test(NAME StockTickerTests COMMAND StockTickerTests)
