cmake_minimum_required(VERSION 3.16)

project(cm_trading_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
     src/common/*.cpp
     src/core/*.cpp
     src/matching/*.cpp
     src/risk/*.cpp
     src/oms/*.cpp
     src/exchange/*.cpp
     src/market_data/*.cpp
)

# Standard Exchange Executable (No QuickFIX required)
add_executable(exchange apps/exchange_main.cpp ${SOURCES})

# FIX Exchange Acceptor Executable (Requires QuickFIX)
file(GLOB FIX_SOURCES src/fix/*.cpp)
add_executable(exchange_fix apps/exchange_fix_main.cpp ${SOURCES} ${FIX_SOURCES})

target_include_directories(exchange_fix PRIVATE "C:/quickfix-master/include")
target_link_directories(exchange_fix PRIVATE "C:/quickfix-master/lib")
target_link_libraries(exchange_fix PRIVATE quickfix ws2_32 wsock32 odbc32)

