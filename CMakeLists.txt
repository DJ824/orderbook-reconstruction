cmake_minimum_required(VERSION 3.27)

project(databento_orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

set(BOOST_ROOT "/opt/homebrew/opt/boost")

set(Boost_INCLUDE_DIR "${BOOST_ROOT}/include")
set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib")

set(PQXX_ROOT "/opt/homebrew/Cellar/libpqxx/7.9.2")
set(PQXX_INCLUDE_DIR "${PQXX_ROOT}/include")
set(PQXX_LIBRARY_DIR "${PQXX_ROOT}/lib")

set(JSON_ROOT "/opt/homebrew/Cellar/nlohmann-json/3.11.3")
set(JSON_INCLUDE_DIR "${JSON_ROOT}/include")

set(CURL_ROOT "/opt/homebrew/Cellar/curl/8.9.1")
set(CURL_INCLUDE_DIR "${CURL_ROOT}/include")
set(CURL_LIBRARY_DIR "${CURL_ROOT}/lib")

set(WEBSOCKETPP_INCLUDE_DIR "/usr/local/include")


set(SOURCES
        src/limit.cpp
        src/main.cpp
        src/order.cpp
        src/orderbook.cpp
        src/order_pool.cpp
        include/message.h
        src/parser.cpp
        src/database.cpp
        src/websocket.cpp
)

find_package(Boost 1.75.0 REQUIRED COMPONENTS system filesystem)
find_library(PQXX_LIB pqxx PATHS ${PQXX_LIBRARY_DIR} REQUIRED)
find_library(CURL_LIB curl PATHS ${CURL_LIBRARY_DIR} REQUIRED)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        ${PQXX_INCLUDE_DIR}
        ${JSON_INCLUDE_DIR}
        ${CURL_INCLUDE_DIR}
        ${WEBSOCKETPP_INCLUDE_DIR}

)

add_executable(databento_orderbook ${SOURCES})

target_link_libraries(databento_orderbook PRIVATE
        Boost::boost
        ${PQXX_LIB}
        ${CURL_LIB}
)

target_include_directories(databento_orderbook PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        ${PQXX_INCLUDE_DIR}
        ${JSON_INCLUDE_DIR}
        ${CURL_INCLUDE_DIR}
        ${WEBSOCKETPP_INCLUDE_DIR}

)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
