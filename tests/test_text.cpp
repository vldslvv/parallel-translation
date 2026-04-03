#include <catch2/catch_test_macros.hpp>

#include "text/text.hpp"

// --- split_words: words ---

TEST_CASE("split_words: single word", "[text]") {
    auto st = split_words("hello");
    REQUIRE(st.words == std::vector<std::string>{"hello"});
    REQUIRE(st.separators == std::vector<std::string>{"", ""});
}

TEST_CASE("split_words: two words with space", "[text]") {
    auto st = split_words("hello world");
    REQUIRE(st.words == std::vector<std::string>{"hello", "world"});
    REQUIRE(st.separators == std::vector<std::string>{"", " ", ""});
}

TEST_CASE("split_words: punctuation becomes separator", "[text]") {
    auto st = split_words("hello, world.");
    REQUIRE(st.words == std::vector<std::string>{"hello", "world"});
    REQUIRE(st.separators == std::vector<std::string>{"", ", ", "."});
}

TEST_CASE("split_words: leading separator", "[text]") {
    auto st = split_words("  hello");
    REQUIRE(st.words == std::vector<std::string>{"hello"});
    REQUIRE(st.separators == std::vector<std::string>{"  ", ""});
}

TEST_CASE("split_words: trailing separator", "[text]") {
    auto st = split_words("hello  ");
    REQUIRE(st.words == std::vector<std::string>{"hello"});
    REQUIRE(st.separators == std::vector<std::string>{"", "  "});
}

TEST_CASE("split_words: empty string produces no words and one empty separator", "[text]") {
    auto st = split_words("");
    REQUIRE(st.words.empty());
    REQUIRE(st.separators == std::vector<std::string>{""});
}

TEST_CASE("split_words: separator only string", "[text]") {
    auto st = split_words("... ---");
    REQUIRE(st.words.empty());
    REQUIRE(st.separators == std::vector<std::string>{"... ---"});
}

TEST_CASE("split_words: invariant separators.size() == words.size() + 1", "[text]") {
    for (const std::string& text : {"", "a", "a b", "a b c", "  spaced  ", "no.punct!"}) {
        auto st = split_words(text);
        REQUIRE(st.separators.size() == st.words.size() + 1);
    }
}

// --- split_words: contractions ---

TEST_CASE("split_words: contraction kept as single word", "[text]") {
    auto st = split_words("don't");
    REQUIRE(st.words == std::vector<std::string>{"don't"});
}

TEST_CASE("split_words: contraction mid-sentence", "[text]") {
    auto st = split_words("I don't know.");
    REQUIRE(st.words == std::vector<std::string>{"I", "don't", "know"});
}

TEST_CASE("split_words: trailing apostrophe is not part of word", "[text]") {
    // "dogs'" — possessive apostrophe at end should become a separator
    auto st = split_words("dogs' toy");
    REQUIRE(st.words == std::vector<std::string>{"dogs", "toy"});
    REQUIRE(st.separators[1] == "' ");
}

// --- split_words: accented characters ---

TEST_CASE("split_words: accented Latin characters are part of words", "[text]") {
    auto st = split_words("café naïve");
    REQUIRE(st.words == std::vector<std::string>{"café", "naïve"});
    REQUIRE(st.separators == std::vector<std::string>{"", " ", ""});
}

// --- split_words: numbers ---

TEST_CASE("split_words: digits are not words", "[text]") {
    auto st = split_words("chapter 3 verse 7");
    REQUIRE(st.words == std::vector<std::string>{"chapter", "verse"});
    REQUIRE(st.separators[1] == " 3 ");
    REQUIRE(st.separators[2] == " 7");
}

// --- compare_words ---

TEST_CASE("compare_words: identical ASCII words match", "[text]") {
    REQUIRE(compare_words("hello", "hello"));
}

TEST_CASE("compare_words: empty words match", "[text]") { REQUIRE(compare_words("", "")); }

TEST_CASE("compare_words: different words do not match", "[text]") {
    REQUIRE_FALSE(compare_words("hello", "world"));
}

TEST_CASE("compare_words: different lengths do not match", "[text]") {
    REQUIRE_FALSE(compare_words("hello", "hell"));
    REQUIRE_FALSE(compare_words("hell", "hello"));
    REQUIRE_FALSE(compare_words("a", ""));
    REQUIRE_FALSE(compare_words("", "a"));
}

TEST_CASE("compare_words: macronized vowel matches plain vowel", "[text]") {
    REQUIRE(compare_words("amō", "amo"));
    REQUIRE(compare_words("māter", "mater"));
    REQUIRE(compare_words("rēx", "rex"));
    REQUIRE(compare_words("fīlius", "filius"));
    REQUIRE(compare_words("lūna", "luna"));
}

TEST_CASE("compare_words: macronized vowels match each other", "[text]") {
    REQUIRE(compare_words("amō", "amō"));
    REQUIRE(compare_words("māter", "māter"));
}

TEST_CASE("compare_words: different vowels do not match", "[text]") {
    REQUIRE_FALSE(compare_words("amo", "ame"));
    REQUIRE_FALSE(compare_words("amō", "amē"));
}

TEST_CASE("compare_words: vowel comparison is case-insensitive", "[text]") {
    REQUIRE(compare_words("Ālea", "Alea"));
    REQUIRE(compare_words("Āmor", "amor"));
}

TEST_CASE("compare_words: consonant comparison is case-sensitive", "[text]") {
    REQUIRE_FALSE(compare_words("Bat", "bat"));
    REQUIRE_FALSE(compare_words("Rex", "rex"));
}

// --- reconstruct ---

TEST_CASE("reconstruct: round-trips arbitrary strings", "[text]") {
    for (const std::string& text : {
             "",
             "hello",
             "hello world",
             "hello, world.",
             "  leading and trailing  ",
             "don't stop!",
             "café au lait",
             "... no words here ...",
             "chapter 3: verse 7",
         }) {
        REQUIRE(reconstruct(split_words(text)) == text);
    }
}
