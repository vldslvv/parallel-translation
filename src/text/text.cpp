#include "text.hpp"

#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

SplitText split_words(std::string_view text) {
    // Matches Latin letters including Extended Latin (accented characters).
    // Apostrophes internal to a word are kept together (contractions: don't, it's).
    static const std::regex word_re("[a-zA-Z\u00C0-\u024F]+(?:'[a-zA-Z\u00C0-\u024F]+)*");

    SplitText result;
    size_t last_end = 0;

    auto it = std::cregex_iterator(text.data(), text.data() + text.size(), word_re);
    auto end = std::cregex_iterator();

    for (; it != end; ++it) {
        const auto& match = *it;
        result.separators.push_back(
            std::string(text.data() + last_end, match.position() - last_end));
        result.words.push_back(match.str());
        last_end = match.position() + match.length();
    }

    result.separators.push_back(std::string(text.data() + last_end, text.size() - last_end));
    return result;
}

std::string reconstruct(const SplitText& st) {
    std::string out;
    for (size_t i = 0; i < st.words.size(); ++i)
        out += st.separators[i] + st.words[i];
    out += st.separators.back();
    return out;
}

namespace {
// clang-format off
std::optional<char> to_base_vowel(char32_t cp) {
    switch (cp) {
    case U'ā': case U'a': case U'Ā': case U'A':
        return 'a';
    case U'ē': case U'e': case U'Ē': case U'E':
        return 'e';
    case U'ī': case U'i': case U'Ī': case U'I':
        return 'i';
    case U'ō': case U'o': case U'Ō': case U'O':
        return 'o';
    case U'ū': case U'u': case U'Ū': case U'U':
        return 'u';
    default:
        return std::nullopt;
    }
}

char32_t decode_utf8(std::string::const_iterator& it, std::string::const_iterator end) {
    if (it == end) return 0;

    // Cast to unsigned to avoid sign-extension issues in bit operations
    auto b = static_cast<unsigned char>(*it);

    // Single-byte ASCII (0xxxxxxx) — no decoding needed
    if (b < 0x80) { ++it; return b; }

    uint32_t cp;
    int extra;

    // Determine sequence length from the leading byte pattern:
    //   110xxxxx = 2-byte (U+0080–U+07FF, includes macron vowels)
    //   1110xxxx = 3-byte (U+0800–U+FFFF)
    //   11110xxx = 4-byte (U+10000–U+10FFFF, e.g. emoji)
    // Mask out the prefix bits to get the initial data bits.
    if      ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
    else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
    else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
    else { ++it; return 0; } // Invalid leading byte

    for (int i = 0; i < extra; ++i) {
        ++it;
        // Each continuation byte must match 10xxxxxx
        if (it == end || (*it & 0xC0) != 0x80) return 0;
        // Shift accumulated bits left and append 6 payload bits
        // from this continuation byte
        cp = (cp << 6) | (*it & 0x3F);
    }

    ++it;
    return cp;
}
// clang-format on

bool compare_symbol(char32_t c1, char32_t c2) {
    if (c1 == c2)
        return true;
    auto v1 = to_base_vowel(c1);
    auto v2 = to_base_vowel(c2);
    return v1 && v2 && *v1 == *v2;
}

} // namespace

bool compare_words(const std::string& w1, const std::string& w2) {
    auto it1 = w1.cbegin();
    auto it2 = w2.cbegin();
    while (it1 != w1.cend() && it2 != w2.cend()) {
        char32_t c1 = decode_utf8(it1, w1.cend());
        char32_t c2 = decode_utf8(it2, w2.cend());
        if (!compare_symbol(c1, c2))
            return false;
    }
    return it1 == w1.cend() && it2 == w2.cend();
}
