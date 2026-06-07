#include "reader.hpp"
#include "sentence_splitter.hpp"
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <spdlog/spdlog.h>
#include <string_view>

namespace {

std::generator<std::string> make_chunks(poppler::document* document) {
    for (int i = 0; i < document->pages(); i++) {
        auto page = std::unique_ptr<poppler::page>(document->create_page(i));
        auto page_rect = page->page_rect();
        constexpr double margin_top = 70.0;
        constexpr double margin_bottom = 50.0;
        poppler::rectf body(page_rect.left(), page_rect.top() + margin_top, page_rect.width(),
                            page_rect.height() - (margin_top + margin_bottom));
        auto bytes = page->text(body).to_utf8();
        co_yield std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
}

} // namespace

std::generator<std::expected<std::string, std::string>> pdf_reader(std::string_view path) {
    spdlog::debug("reading file: {}", path);
    auto document =
        std::unique_ptr<poppler::document>(poppler::document::load_from_file(std::string(path)));

    if (document == nullptr) {
        co_yield std::unexpected("Failed to open file: " + std::string(path));
        co_return;
    }

    for (const auto& s : split_sentences(make_chunks(document.get())))
        co_yield s;
}
