#pragma once
#include <string>

struct ChatApiConfig {
    std::string provider = "ollama";
    std::string host;
    std::string model = "gemma3:27b";
    std::string api_key;
};

struct Config {
    ChatApiConfig chat_api;
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    int parallelism = 1;
};

// Loads config from (in order of increasing priority):
//   $XDG_CONFIG_HOME/parallel-translation/config.toml
//   environment variables (PT_CHAT_PROVIDER, PT_CHAT_HOST, PT_CHAT_MODEL,
//   PT_CHAT_API_KEY, PT_SOURCE_LANG, PT_TARGET_LANG)
Config load_config();
