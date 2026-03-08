#pragma once
#include <functional>
#include <string>
#include <string_view>

// A Formatter combines original and translation into output text
using Formatter =
    std::function<std::string(std::string_view original, std::string_view translation)>;

// Plain formatter: "original\ntranslation\n\n"
std::string plain_formatter(std::string_view original, std::string_view translation);
