#pragma once
#include <string_view>
#include <vector>

enum class TokenKind { Word, Separator };

struct Token {
    std::string_view text;
    TokenKind kind;
};

inline bool is_separator(char c) {
    return c == ' ' || c == ',' || c == '.' || c == '!'
        || c == '?' || c == ';' || c == ':' || c == '-'
        || c == '(' || c == ')' || c == '[' || c == ']'
        || c == '"' || c == '\'' || c == '\n' || c == '\t';
}

inline std::vector<Token> tokenize(std::string_view text) {
    std::vector<Token> tokens;
    std::size_t start = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (is_separator(text[i])) {
            if (i > start)
                tokens.push_back({text.substr(start, i - start), TokenKind::Word});
            tokens.push_back({text.substr(i, 1), TokenKind::Separator});
            start = i + 1;
        }
    }
    if (start < text.size())
        tokens.push_back({text.substr(start), TokenKind::Word});

    return tokens;
}
