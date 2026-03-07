cmake_minimum_required(VERSION 3.20)

project(parallel-translation
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(CLI11 Catch2)

add_executable(parallel-translation
    src/main.cpp
    src/app.cpp
    src/translators/stub.cpp
    src/readers/txt.cpp
    src/writers/txt.cpp
)

target_include_directories(parallel-translation PRIVATE src)
target_link_libraries(parallel-translation PRIVATE CLI11::CLI11)

target_compile_options(parallel-translation PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()
add_subdirectory(tests)
