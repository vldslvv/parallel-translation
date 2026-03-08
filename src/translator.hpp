#pragma once
#include <functional>
#include <string>
#include <string_view>

// A Translator is any callable: std::string(std::string_view)
using Translator = std::function<std::string(std::string_view)>;

// Stub implementation — returns input unchanged
std::string stub_translator(std::string_view text);

// Pass-through implementation — returns input unchanged
std::string pass_translator(std::string_view text);
