cmake_minimum_required(VERSION 4.0)
project(minibase)

set(CMAKE_CXX_FLGAS "-Wall -O0")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_BUILD_TYPE Debug)

option(ENABLE_ASAN "Enable Address Sanitizer" ON)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(databases_project src/main.cpp)
