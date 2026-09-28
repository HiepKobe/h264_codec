cmake_minimum_required(VERSION 3.18)

project(h264_bridge LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# 1. Optional libyuv Integration via CMake HTTP Download (No git dependency)
# -----------------------------------------------------------------------------
set(LIBYUV_DIR "${CMAKE_CURRENT_BINARY_DIR}/libyuv-src")

if(NOT EXISTS "${LIBYUV_DIR}/include/libyuv.h")
    message(STATUS "Downloading libyuv source archive...")
    file(DOWNLOAD
        "https://github.com/libyuv/libyuv/archive/refs/heads/master.zip"
        "${CMAKE_CURRENT_BINARY_DIR}/libyuv.zip"
        STATUS DOWNLOAD_STATUS
        SHOW_PROGRESS
    )
    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
    if(DOWNLOAD_CODE EQUAL 0 AND EXISTS "${CMAKE_CURRENT_BINARY_DIR}/libyuv.zip")
        file(ARCHIVE_EXTRACT
            INPUT "${CMAKE_CURRENT_BINARY_DIR}/libyuv.zip"
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
        )
        if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/libyuv-master")
            file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/libyuv-master" "${LIBYUV_DIR}")
        endif()
    else()
        message(WARNING "Could not download libyuv archive. Falling back to internal C++ converter.")
    endif()
endif()

if(EXISTS "${LIBYUV_DIR}/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${LIBYUV_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libyuv-build" EXCLUDE_FROM_ALL)
endif()

# Enable ARM NEON optimizations for Android
if(ANDROID_ABI MATCHES "armeabi-v7a" OR ANDROID_ABI MATCHES "arm64-v8a")
    add_definitions(-DLIBYUV_NEON)
endif()

# -----------------------------------------------------------------------------
# 2. Add h264_bridge Target
# -----------------------------------------------------------------------------
add_library(h264_bridge SHARED
    ../src/h264_bridge.cpp
    ../src/h264_bridge.h
    ../src/h264_hardware_codec.cpp
    ../src/h264_hardware_codec.h
    ../src/pixel_converter_libyuv.cpp
    ../src/pixel_converter_libyuv.h
)

find_library(log-lib log)
find_library(mediandk-lib mediandk)

if(TARGET yuv)
    target_include_directories(h264_bridge PUBLIC
        ../src
        ${LIBYUV_DIR}/include
    )
    target_link_libraries(h264_bridge
        yuv
        ${log-lib}
        ${mediandk-lib}
    )
else()
    target_include_directories(h264_bridge PUBLIC
        ../src
    )
    target_link_libraries(h264_bridge
        ${log-lib}
        ${mediandk-lib}
    )
endif()
