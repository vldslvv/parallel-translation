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
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.1
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(CLI11 Catch2 httplib nlohmann_json tomlplusplus spdlog)

add_executable(parallel-translation
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/translators/stub.cpp
    src/translators/ollama.cpp
    src/readers/txt.cpp
    src/writers/txt.cpp
)

target_include_directories(parallel-translation PRIVATE src)
target_link_libraries(parallel-translation PRIVATE
    CLI11::CLI11 httplib::httplib nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus spdlog::spdlog
)

target_compile_options(parallel-translation PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()
add_subdirectory(tests)
