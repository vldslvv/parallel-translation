#pragma once
#include <string>

struct Config {
    std::string ollama_host = "http://localhost:11434";
    std::string ollama_model = "gemma3:27b";
    std::string morpheus_dir;
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    int parallelism = 1;
};

// Loads config from (in order of increasing priority):
//   $XDG_CONFIG_HOME/parallel-translation/config.toml
//   environment variables (PT_OLLAMA_HOST, PT_OLLAMA_MODEL, PT_MORPHEUS_DIR,
//   PT_SOURCE_LANG, PT_TARGET_LANG)
Config load_config();
