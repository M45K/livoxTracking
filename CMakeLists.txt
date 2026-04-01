cmake_minimum_required(VERSION 3.16)
project(livox_tracking_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_LIVOX "Build with Livox SDK2 support" ON)

set(APP_SOURCES
  src/main.cpp
  src/config.cpp
  src/livox_receiver.cpp
  src/background.cpp
  src/debug_view.cpp
  src/point_processing.cpp
  src/tracker.cpp
  src/zones.cpp
  src/net.cpp
  src/tcp_json_publisher.cpp
  src/osc_publisher.cpp
)

add_executable(livox_tracking_service ${APP_SOURCES})
target_include_directories(livox_tracking_service PRIVATE src)
target_compile_definitions(livox_tracking_service PRIVATE _USE_MATH_DEFINES)

if(USE_LIVOX)
  if(NOT LIVOX_INCLUDE)
    find_path(
      LIVOX_INCLUDE
      livox_lidar_api.h
      PATHS
        $ENV{LIVOX_SDK}/include
        ${LIVOX_SDK}/include
        /usr/include
        /usr/local/include
    )
  endif()

  if(NOT LIVOX_LIB)
    find_library(
      LIVOX_LIB
      NAMES livox_lidar_sdk_static livox_lidar_sdk_shared livox_sdk
      PATHS
        $ENV{LIVOX_SDK}/build/sdk_core/Release
        ${LIVOX_SDK}/build/sdk_core/Release
        $ENV{LIVOX_SDK}/build/sdk_core/Debug
        ${LIVOX_SDK}/build/sdk_core/Debug
        $ENV{LIVOX_SDK}/lib
        ${LIVOX_SDK}/lib
        /usr/lib
        /usr/local/lib
    )
  endif()

  if(LIVOX_INCLUDE AND LIVOX_LIB)
    message(STATUS "Found Livox SDK include: ${LIVOX_INCLUDE}")
    message(STATUS "Found Livox SDK lib: ${LIVOX_LIB}")
    target_include_directories(livox_tracking_service PRIVATE ${LIVOX_INCLUDE})
    target_link_libraries(livox_tracking_service PRIVATE ${LIVOX_LIB})
    target_compile_definitions(livox_tracking_service PRIVATE WITH_LIVOX)
  else()
    message(FATAL_ERROR "Livox SDK2 not found. Set LIVOX_SDK or LIVOX_INCLUDE/LIVOX_LIB.")
  endif()
endif()

if(WIN32)
  target_link_libraries(livox_tracking_service PRIVATE ws2_32)
endif()
