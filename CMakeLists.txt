cmake_minimum_required(VERSION 3.5.0)
project(json)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_STANDARD 99)

include_directories(.)
link_directories(./cJSON)

add_library(json SHARED
        json.c)

set_property(TARGET json PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(json yaslapi)
target_link_libraries(json "${CMAKE_SOURCE_DIR}/cJSON/libcjson.a")
