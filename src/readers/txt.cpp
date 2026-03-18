#include "reader.hpp"
#include "sentence_splitter.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

static std::generator<std::string> make_chunks(std::ifstream& file) {
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        co_yield line.empty() ? std::string{"\n\n"} : line + "\n";
    }
}

std::generator<std::expected<std::string, std::string>> txt_reader(std::string_view path) {
    spdlog::debug("reading file: {}", path);
    std::ifstream file{std::string{path}};
    if (!file) {
        co_yield std::unexpected{"cannot open file: " + std::string{path}};
        co_return;
    }

    for (const auto& s : split_sentences(make_chunks(file)))
        co_yield s;
}
