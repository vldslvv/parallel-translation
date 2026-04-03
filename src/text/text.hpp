#pragma once

#include <string>
#include <string_view>
#include <vector>

struct SplitText {
    std::vector<std::string> words;
    std::vector<std::string> separators; // always words.size() + 1
    // reconstruct: sep[0] + word[0] + sep[1] + word[1] + ... + sep[n]
};

SplitText split_words(std::string_view text);
std::string reconstruct(const SplitText& st);
bool compare_words(const std::string& w1, const std::string& w2);
