#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "app.hpp"
#include "common/exit_codes.hpp"
#include "test_helpers.hpp"

static const char* INPUT = ASSETS_DIR "/latin_example.txt";
static const char* OUTPUT = ASSETS_DIR "/latin_example_out.txt";

TEST_CASE("translates input file to output file", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub", "--postprocess", "none",
                          "-i",  INPUT,      "-o",            OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto out = read_file(OUTPUT);
    CHECK(!out.empty());
    // stub replaces every word with "Stub", separators are preserved
    CHECK(out.contains("Stub"));

    std::remove(OUTPUT);
}

TEST_CASE("errors on non-existent input file", "[integration]") {
    const char* argv[] = {"app",
                          "--backend",
                          "stub",
                          "--postprocess",
                          "none",
                          "-i",
                          ASSETS_DIR // NOLINT(bugprone-suspicious-missing-comma)
                          "/nonexistent.txt",
                          "-o",
                          OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == exit_code::input_error);
}

TEST_CASE("errors when parallelism exceeds semaphore capacity", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub",          "--postprocess", "none",
                          "-i",  INPUT,      "-o",            OUTPUT,         "--parallelism",
                          "1025"};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == exit_code::usage_error);

    std::remove(OUTPUT);
}
