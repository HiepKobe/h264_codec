cmake_minimum_required(VERSION 3.10.2)

project(h264_bridge LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(h264_bridge SHARED
    ../src/h264_bridge.cpp
    ../src/h264_bridge.h
)

target_include_directories(h264_bridge PUBLIC
    ../src
)

find_library(log-lib log)

target_link_libraries(h264_bridge
    ${log-lib}
)
