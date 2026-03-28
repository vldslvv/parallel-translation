#include "writers/pdf.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>

TEST_CASE("pdf writer creates a file", "[pdf]") {
    const std::string out = std::string(ASSETS_DIR) + "/test_output.pdf";
    {
        PdfWriter pw(out);
        REQUIRE(pw.is_open());
        CHECK(pw.write_pair("Hello world.", "Hola mundo.") == 0);
    }
    REQUIRE(std::filesystem::exists(out));
    REQUIRE(std::filesystem::file_size(out) > 0);
    std::remove(out.c_str());
}

TEST_CASE("pdf formatted writer interface", "[pdf]") {
    const std::string out = std::string(ASSETS_DIR) + "/test_formatted.pdf";
    {
        PdfWriter pw(out);
        REQUIRE(pw.is_open());
        auto fw = make_pdf_formatted_writer(pw);
        CHECK(fw("Original sentence.", "Translated sentence.") == 0);
    }
    REQUIRE(std::filesystem::exists(out));
    std::remove(out.c_str());
}
