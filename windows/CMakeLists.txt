cmake_minimum_required(VERSION 3.14)
project(h264_codec_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(h264_bridge SHARED
    "../src/h264_bridge.cpp"
    "../src/h264_bridge.h"
)

target_compile_definitions(h264_bridge PRIVATE H264_BRIDGE_EXPORTS)

target_include_directories(h264_bridge PUBLIC
    "../src"
)

set_target_properties(h264_bridge PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
