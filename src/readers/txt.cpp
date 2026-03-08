#include "reader.hpp"
#include <fstream>

#include <spdlog/spdlog.h>

std::generator<std::expected<std::string, std::string>> txt_reader(std::string_view path) {
    spdlog::debug("reading file: {}", path);
    std::ifstream file{std::string{path}};
    if (!file) {
        co_yield std::unexpected{"cannot open file: " + std::string{path}};
        co_return;
    }

    std::string sentence;
    char c;
    while (file.get(c)) {
        if (sentence.empty() && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            continue;
        }
        sentence += c;
        if (c == '.' || c == '!' || c == '?') {
            spdlog::debug("yielding sentence ({} bytes)", sentence.size());
            co_yield sentence;
            sentence.clear();
        }
    }
    if (sentence.find_first_not_of(" \t\r\n") != std::string::npos)
        co_yield sentence;
}
