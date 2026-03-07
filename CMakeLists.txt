cmake_minimum_required(VERSION 3.20)

project(parallel-translation
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(parallel-translation
    src/main.cpp
)

target_compile_options(parallel-translation PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
