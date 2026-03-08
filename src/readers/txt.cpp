#include "reader.hpp"
#include <fstream>

#include <spdlog/spdlog.h>

static void trim_trailing_spaces(std::string& s) {
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
}

std::generator<std::expected<std::string, std::string>> txt_reader(std::string_view path) {
    spdlog::debug("reading file: {}", path);
    std::ifstream file{std::string{path}};
    if (!file) {
        co_yield std::unexpected{"cannot open file: " + std::string{path}};
        co_return;
    }

    std::string sentence;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty()) {
            trim_trailing_spaces(sentence);
            if (!sentence.empty()) {
                spdlog::debug("yielding sentence ({} bytes)\n{}", sentence.size(), sentence);
                co_yield sentence;
                sentence.clear();
            }
            continue;
        }

        if (!sentence.empty())
            sentence += ' ';

        for (char c : line) {
            if (sentence.empty() && (c == ' ' || c == '\t'))
                continue;
            sentence += c;
            if (c == '.' || c == '!' || c == '?') {
                spdlog::debug("yielding sentence ({} bytes)\n{}", sentence.size(), sentence);
                co_yield sentence;
                sentence.clear();
            }
        }
    }

    trim_trailing_spaces(sentence);
    if (!sentence.empty()) {
        spdlog::debug("yielding sentence ({} bytes)\n{}", sentence.size(), sentence);
        co_yield sentence;
    }
}
