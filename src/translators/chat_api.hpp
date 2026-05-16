#pragma once
#include "config.hpp"
#include "translator.hpp"

Translator make_chat_api_latin_to_english_translator(const ChatApiConfig& cfg);

Translator make_chat_api_macron_translator(const ChatApiConfig& cfg);
