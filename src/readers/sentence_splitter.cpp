#include "sentence_splitter.hpp"
#include <spdlog/spdlog.h>

std::generator<std::string> split_sentences(std::generator<std::string> chunks) { // NOLINT(readability-function-cognitive-complexity)
    std::string sentence;
    bool prev_was_newline = false;

    for (const auto& chunk : chunks) {
        for (char c : chunk) {
            if (c == '\n') {
                if (prev_was_newline) {
                    while (!sentence.empty() && sentence.back() == ' ')
                        sentence.pop_back();
                    if (!sentence.empty()) {
                        spdlog::debug("yielding sentence ({} bytes)\n{}", sentence.size(), sentence);
                        co_yield sentence;
                        sentence.clear();
                    }
                } else {
                    if (!sentence.empty())
                        sentence += ' ';
                }
                prev_was_newline = true;
                continue;
            }
            prev_was_newline = false;
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

    while (!sentence.empty() && sentence.back() == ' ')
        sentence.pop_back();
    if (!sentence.empty()) {
        spdlog::debug("yielding sentence ({} bytes)\n{}", sentence.size(), sentence);
        co_yield sentence;
    }
}
