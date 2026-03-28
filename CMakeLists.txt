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
find_package(PDFHummus REQUIRED)
find_package(poppler REQUIRED)

add_executable(parallel-translation
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/translators/stub.cpp
    src/translators/pass.cpp
    src/translators/ollama.cpp
    src/readers/sentence_splitter.cpp
    src/readers/txt.cpp
    src/readers/pdf.cpp
    src/writers/txt.cpp
    src/writers/pdf.cpp
    src/writers/formatted_writer.cpp
    src/formatters/plain.cpp
)

install(TARGETS parallel-translation RUNTIME DESTINATION bin)

# freetype is an explicit Conan dependency (conan.io/center/recipes/freetype).
# Its Conan recipe intentionally omits INTERFACE_INCLUDE_DIRECTORIES from the
# generated Freetype::Freetype target (it treats FreeType as a system library),
# so linking against it alone does not add the headers to the include path.
# The Conan toolchain populates CMAKE_INCLUDE_PATH with the package's own
# include dirs, so find_path resolves ft2build.h from the Conan-managed
# package — no system path is assumed.
find_path(FREETYPE_INCLUDE_DIR NAMES ft2build.h)
if(NOT FREETYPE_INCLUDE_DIR)
    message(FATAL_ERROR "Cannot find ft2build.h — is freetype installed via Conan?")
endif()
target_include_directories(parallel-translation PRIVATE src "${FREETYPE_INCLUDE_DIR}")
# Freetype::Freetype is listed explicitly for the same reason: to ensure the
# linker resolves FreeType symbols that PDFHummus uses without relying solely
# on transitive linkage.
target_link_libraries(parallel-translation PRIVATE
    CLI11::CLI11 httplib::httplib nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus spdlog::spdlog PDFHummus::PDFWriter
    Freetype::Freetype poppler::poppler
)

target_compile_options(parallel-translation PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    # /home/vv is a symlink to /mnt/shared_linux/home_shared, causing the compiler
    # to embed symlink paths in debug info that the debugger can't resolve.
    # This rewrites them to the real path at compile time.
    $<$<CONFIG:Debug>:-fdebug-prefix-map=/home/vv=/mnt/shared_linux/home_shared>
)

enable_testing()
add_subdirectory(tests)
