#pragma once
#include <expected>
#include <string>

struct OllamaConfig {
    std::string host = "http://localhost:11434";
    std::string model = "gemma3:27b";
    std::string api_key;
};

struct OpenRouterConfig {
    std::string host = "https://openrouter.ai";
    std::string model = "google/gemma-4-31b-it";
    std::string api_key;
};

struct Config {
    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    std::string chat_api_provider = "ollama";
    OllamaConfig ollama;
    OpenRouterConfig openrouter;
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    bool breves = false;
    int parallelism = 1;
};

struct ChatApiConfig {
    std::string provider;
    std::string host;
    std::string model;
    std::string api_key;
};

// Loads config from (in order of increasing priority):
//   built-in provider defaults
//   $XDG_CONFIG_HOME/parallel-translation/config.toml
//   environment variables (PT_CHAT_PROVIDER, PT_CHAT_HOST, PT_CHAT_MODEL,
//   PT_CHAT_API_KEY, PT_SOURCE_LANG, PT_TARGET_LANG, PT_LOG_LEVEL, PT_PARALLELISM)
Config load_config();

std::expected<Config, int> get_config(int argc, char* argv[]);

ChatApiConfig selected_chat_api_config(const Config& cfg);
