cmake_minimum_required(VERSION 4.2)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        Networking/OrderListener.cpp)

target_include_directories(OrderBook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(OrderBook PRIVATE PkgConfig::PROTOBUF)
