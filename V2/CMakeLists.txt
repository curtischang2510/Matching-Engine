cmake_minimum_required(VERSION 4.2)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ORDERBOOK_ENABLE_LOGGING "Enable async order book logging" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)

add_executable(OrderBook
        main.cpp
        OrderBook.cpp
        OrderBook.h
        Exchange.cpp
        Exchange.h
        Trade.h
        generated/order.pb.cc
        generated/order.pb.h
        Networking/OrderProtoAdapter.cpp
        Networking/OrderProtoAdapter.h
        Networking/ProtobufFrameReader.cpp
        Networking/ProtobufFrameReader.h
        Networking/SocketHandle.h
        Networking/TcpServer.cpp
        Networking/TcpServer.h
        Networking/OrderListener.cpp
        timer/SimpleTimer.cpp
        timer/SimpleTimer.h
        timer/TimeCall.h
        main.cpp
        CircularBuffer/CircularBuffer.cpp
        CircularBuffer/CircularBuffer.h
        Logger/SPSCBuffer.h
        Logger/LogEvent.h
        Logger/LoggingConfig.h
        Logger/AsyncLogger.cpp
        Logger/AsyncLogger.h
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h)

target_compile_definitions(OrderBook PRIVATE
        ORDERBOOK_ENABLE_LOGGING=$<BOOL:${ORDERBOOK_ENABLE_LOGGING}>)
target_include_directories(OrderBook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(OrderBook PRIVATE PkgConfig::PROTOBUF)

enable_testing()

add_executable(OrderBookTests
        tests/OrderBookTests.cpp
        OrderBook.cpp
        OrderBook.h
        Trade.h
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h)

add_test(NAME OrderBookTests COMMAND OrderBookTests)

add_executable(LoggerTests
        tests/LoggerTests.cpp
        Logger/AsyncLogger.cpp
        Logger/AsyncLogger.h
        Logger/LogEvent.h
        Logger/SPSCBuffer.h
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h)

target_include_directories(LoggerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME LoggerTests COMMAND LoggerTests)

add_executable(StockTickerTests
        tests/StockTickerTests.cpp
        StockTicker/StockTicker.cpp
        StockTicker/StockTicker.h)

add_test(NAME StockTickerTests COMMAND StockTickerTests)
