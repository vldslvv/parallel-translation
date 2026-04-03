#pragma once
#include "translator.hpp"
#include <string>

Translator
make_ollama_latin_to_english_translator(const std::string& model,
                                        const std::string& host = "http://localhost:11434");

Translator make_ollama_macron_translator(const std::string& model,
                                         const std::string& host = "http://localhost:11434");
