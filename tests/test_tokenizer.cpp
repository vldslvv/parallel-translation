#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <string>

#include "app.hpp"
#include "test_helpers.hpp"

TEST_CASE("tokenizer: basic sentence separators preserved", "[integration]") {
    const char* in = ASSETS_DIR "/tmp_basic_in.txt";
    const char* out = ASSETS_DIR "/tmp_basic_out.txt";
    {
        std::ofstream f{in};
        f << "Hello, world.";
    }

    const char* argv[] = {"app", "--backend", "stub", "-i", in, "-o", out};
    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto result = read_file(out);
    CHECK(result.contains("Stub, Stub."));

    std::remove(in);
    std::remove(out);
}

TEST_CASE("tokenizer: empty input produces empty output", "[integration]") {
    const char* in = ASSETS_DIR "/tmp_empty_in.txt";
    const char* out = ASSETS_DIR "/tmp_empty_out.txt";
    {
        std::ofstream f{in};
    }

    const char* argv[] = {"app", "--backend", "stub", "-i", in, "-o", out};
    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    CHECK(read_file(out).empty());

    std::remove(in);
    std::remove(out);
}

TEST_CASE("tokenizer: single word", "[integration]") {
    const char* in = ASSETS_DIR "/tmp_word_in.txt";
    const char* out = ASSETS_DIR "/tmp_word_out.txt";
    {
        std::ofstream f{in};
        f << "word";
    }

    const char* argv[] = {"app", "--backend", "stub", "-i", in, "-o", out};
    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto result = read_file(out);
    CHECK(result.contains("Stub"));

    std::remove(in);
    std::remove(out);
}

TEST_CASE("tokenizer: dashes and question marks preserved", "[integration]") {
    const char* in = ASSETS_DIR "/tmp_dash_in.txt";
    const char* out = ASSETS_DIR "/tmp_dash_out.txt";
    {
        std::ofstream f{in};
        f << "Why-not? Yes!";
    }

    const char* argv[] = {"app", "--backend", "stub", "-i", in, "-o", out};
    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto result = read_file(out);
    CHECK(result.contains("Stub-Stub?"));
    CHECK(result.contains("Stub!"));

    std::remove(in);
    std::remove(out);
}
