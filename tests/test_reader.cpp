#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <string>

#include "app.hpp"
#include "test_helpers.hpp"

static void write_file(const char* path, const std::string& content) {
    std::ofstream f{path};
    f << content;
}

static std::string run_pass(const char* in, const char* out) {
    const char* argv[] = {"app",  "--backend-provider", "pass", "--postprocessor-provider",
                          "none", "--reader-path",      in,     "--writer-path",
                          out};
    run(std::size(argv), const_cast<char**>(argv));
    return read_file(out);
}

TEST_CASE("reader: two sentences", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_two_sentences_in.txt";
    const char* OUT = ASSETS_DIR "/reader_two_sentences_out.txt";
    write_file(IN, "First sentence. Second sentence.");
    auto result = run_pass(IN, OUT);
    // Each sentence should be yielded separately: output contains "S\nS" for each
    CHECK(result.contains("First sentence.\nFirst sentence."));
    CHECK(result.contains("Second sentence.\nSecond sentence."));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: single line break inside sentence becomes space", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_single_newline_in.txt";
    const char* OUT = ASSETS_DIR "/reader_single_newline_out.txt";
    write_file(IN, "Hello\nworld.");
    auto result = run_pass(IN, OUT);
    // Desired behavior: newline normalized to space
    CHECK(result.contains("Hello world."));
    CHECK(!result.contains("Hello\nworld."));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: double line break splits into separate sentences", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_double_newline_in.txt";
    const char* OUT = ASSETS_DIR "/reader_double_newline_out.txt";
    write_file(IN, "First sentence\n\nSecond sentence.");
    auto result = run_pass(IN, OUT);
    // Desired behavior: double newline causes two separate yields
    CHECK(result.contains("First sentence\nFirst sentence"));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: long comma-separated sentence read as one", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_comma_in.txt";
    const char* OUT = ASSETS_DIR "/reader_comma_out.txt";
    write_file(IN, "One, two, three, four.");
    auto result = run_pass(IN, OUT);
    // Entire comma-separated sentence must appear as a single unit
    CHECK(result.contains("One, two, three, four.\nOne, two, three, four."));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: exclamation mark terminates sentence", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_exclamation_in.txt";
    const char* OUT = ASSETS_DIR "/reader_exclamation_out.txt";
    write_file(IN, "What a day!");
    auto result = run_pass(IN, OUT);
    CHECK(result.contains("What a day!\nWhat a day!"));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: question mark terminates sentence", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_question_in.txt";
    const char* OUT = ASSETS_DIR "/reader_question_out.txt";
    write_file(IN, "Is it working?");
    auto result = run_pass(IN, OUT);
    CHECK(result.contains("Is it working?\nIs it working?"));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: leading whitespace before first sentence is stripped", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_leading_ws_in.txt";
    const char* OUT = ASSETS_DIR "/reader_leading_ws_out.txt";
    write_file(IN, "\n\n   Hello world.");
    auto result = run_pass(IN, OUT);
    CHECK(result.contains("Hello world.\nHello world."));
    // Output must not start with whitespace before "Hello"
    CHECK(!result.starts_with("\n"));
    CHECK(!result.starts_with(" "));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: sentence without terminating punctuation yielded at EOF", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_no_punct_in.txt";
    const char* OUT = ASSETS_DIR "/reader_no_punct_out.txt";
    write_file(IN, "No period at end");
    auto result = run_pass(IN, OUT);
    CHECK(result.contains("No period at end\nNo period at end"));
    std::remove(IN);
    std::remove(OUT);
}

TEST_CASE("reader: CRLF within sentence normalized to space", "[integration]") {
    const char* IN = ASSETS_DIR "/reader_crlf_in.txt";
    const char* OUT = ASSETS_DIR "/reader_crlf_out.txt";
    write_file(IN, "Hello\r\nworld.");
    auto result = run_pass(IN, OUT);
    // Desired behavior: CRLF normalized to space
    CHECK(result.contains("Hello world."));
    CHECK(!result.contains("Hello\r\nworld."));
    std::remove(IN);
    std::remove(OUT);
}
