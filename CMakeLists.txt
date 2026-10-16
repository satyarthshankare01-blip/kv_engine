cmake_minimum_required(VERSION 3.20)
project(KVStore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(server
        src/main.cpp
        src/server.cpp
        src/parser.cpp
        include/command.h
        include/parser.h
        include/store.h
        include/server.h
        src/store.cpp
        src/benchmark.cpp
)