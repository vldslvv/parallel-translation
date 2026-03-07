#include <catch2/catch_test_macros.hpp>

#include "translator.hpp"

TEST_CASE("stub_translator replaces words with 'Stub'", "[translator]") {
    CHECK(stub_translator("hello") == "Stub");
    CHECK(stub_translator("Hello, world!") == "Stub, Stub!");
    CHECK(stub_translator("") == "");
}
