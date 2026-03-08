#include "common/tokenizer.hpp"
#include "translator.hpp"

std::string stub_translator(std::string_view text) {
    std::string result;
    for (const auto& token : tokenize(text))
        result += (token.kind == TokenKind::Word) ? "Stub" : token.text;
    return result;
}
