#pragma once
#include "translator.hpp"
#include <string>

Translator make_ollama_translator(std::string model, std::string host = "http://localhost:11434");
