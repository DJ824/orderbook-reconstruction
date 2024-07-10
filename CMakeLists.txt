cmake_minimum_required(VERSION 3.27)

project(databento_orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(databento_orderbook
        main.cpp
        order.cpp
        orderbook.cpp
        limit.cpp
        parser.cpp
        limit_pool.cpp
        limit_pool.h
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")