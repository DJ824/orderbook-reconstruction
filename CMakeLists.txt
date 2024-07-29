cmake_minimum_required(VERSION 3.27)

project(databento_orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")


set(BOOST_ROOT "/opt/homebrew/opt/boost")
set(Boost_INCLUDE_DIR "${BOOST_ROOT}/include")
set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib")

set(SOURCES
        src/limit.cpp
        src/main.cpp
        src/order.cpp
        src/orderbook.cpp
        src/order_pool.cpp
        include/message.h
        src/parser.cpp
)

find_package(Boost 1.75.0 REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)



add_executable(databento_orderbook ${SOURCES})


target_link_libraries(databento_orderbook PRIVATE Boost::boost)
target_include_directories(databento_orderbook PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")