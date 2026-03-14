cmake_minimum_required(VERSION 3.20)

project(parallel-translation
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CLI11 REQUIRED)
find_package(Catch2 REQUIRED)
find_package(httplib REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(spdlog REQUIRED)

add_executable(parallel-translation
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/translators/stub.cpp
    src/translators/pass.cpp
    src/translators/ollama.cpp
    src/readers/txt.cpp
    src/writers/txt.cpp
    src/writers/formatted_writer.cpp
    src/formatters/plain.cpp
)

install(TARGETS parallel-translation RUNTIME DESTINATION bin)

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
