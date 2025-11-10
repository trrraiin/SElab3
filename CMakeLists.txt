cmake_minimum_required(VERSION 3.8)
project(SimpleLedgerCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(simple_ledger
    src/main.cpp
)
