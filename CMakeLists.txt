cmake_minimum_required(VERSION 3.16)

project(cm_trading_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES
     src/common/*.cpp
     src/core/*.cpp
     src/matching/*.cpp
     src/risk/*.cpp
     src/oms/*.cpp
     src/exchange/*.cpp
     src/fix/*.cpp
     src/market_data/*.cpp
)

add_executable(exchange apps/exchange_main.cpp ${SOURCES})
