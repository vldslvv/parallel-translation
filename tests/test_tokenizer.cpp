#include <catch2/catch_test_macros.hpp>

#include "tokenizer.hpp"

TEST_CASE("tokenize splits words and separators", "[tokenizer]") {
    SECTION("basic sentence") {
        auto tokens = tokenize("Hello, world.");
        REQUIRE(tokens.size() == 5);
        CHECK(tokens[0].text == "Hello");
        CHECK(tokens[0].kind == TokenKind::Word);
        CHECK(tokens[1].text == ",");
        CHECK(tokens[1].kind == TokenKind::Separator);
        CHECK(tokens[2].text == " ");
        CHECK(tokens[2].kind == TokenKind::Separator);
        CHECK(tokens[3].text == "world");
        CHECK(tokens[3].kind == TokenKind::Word);
        CHECK(tokens[4].text == ".");
        CHECK(tokens[4].kind == TokenKind::Separator);
    }

    SECTION("empty input") { CHECK(tokenize("").empty()); }

    SECTION("single word") {
        auto tokens = tokenize("word");
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0].text == "word");
        CHECK(tokens[0].kind == TokenKind::Word);
    }

    SECTION("dashes and question marks") {
        auto tokens = tokenize("Why-not? Yes!");
        REQUIRE(tokens.size() == 7);
        CHECK(tokens[0].text == "Why");
        CHECK(tokens[0].kind == TokenKind::Word);
        CHECK(tokens[1].text == "-");
        CHECK(tokens[1].kind == TokenKind::Separator);
        CHECK(tokens[2].text == "not");
        CHECK(tokens[2].kind == TokenKind::Word);
        CHECK(tokens[3].text == "?");
        CHECK(tokens[3].kind == TokenKind::Separator);
        CHECK(tokens[4].text == " ");
        CHECK(tokens[4].kind == TokenKind::Separator);
        CHECK(tokens[5].text == "Yes");
        CHECK(tokens[5].kind == TokenKind::Word);
        CHECK(tokens[6].text == "!");
        CHECK(tokens[6].kind == TokenKind::Separator);
    }
}
