#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <string>

#include "app.hpp"

static const char* INPUT  = ASSETS_DIR "/latin_example.txt";
static const char* OUTPUT = ASSETS_DIR "/latin_example_out.txt";

static std::string read_file(const char* path) {
    std::ifstream f{path};
    return {std::istreambuf_iterator<char>{f}, {}};
}

TEST_CASE("translates input file to output file", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub", "-i", INPUT, "-o", OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto out = read_file(OUTPUT);
    CHECK(!out.empty());
    // stub replaces every word with "Stub", separators are preserved
    CHECK(out.find("Stub") != std::string::npos);
    CHECK(out.find("Cogito") == std::string::npos);

    std::remove(OUTPUT);
}

TEST_CASE("errors on non-existent input file", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub", "-i", ASSETS_DIR "/nonexistent.txt", "-o", OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 1);
}
