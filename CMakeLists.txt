cmake_minimum_required(VERSION 3.27)

project(databento_orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(BOOST_ROOT "/opt/homebrew/opt/boost")
set(Boost_INCLUDE_DIR "${BOOST_ROOT}/include")
set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib")

find_package(Boost 1.75.0 REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIRS})


add_executable(databento_orderbook
        main.cpp
        order.cpp
        orderbook.cpp
        limit.cpp

)

target_link_libraries(databento_orderbook ${Boost_LIBRARIES})
link_directories(${Boost_LIBRARY_DIR})


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")