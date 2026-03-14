cmake_minimum_required(VERSION 4.0)
project(minibase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wformat=2
)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(ENABLE_ASAN "Enable Address Sanitizer" ON)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(databases_project
    source/main.cpp source/parsing_functions.cpp source/csv_parsing_functions.cpp
    source/Database.cpp)
