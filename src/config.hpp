#pragma once
#include <expected>
#include <string>

struct ChatApiConfig {
    std::string provider;
    std::string host;
    std::string model;
    std::string api_key;
    std::string api_style;
    std::string endpoint_path;
};

struct Config {
    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    ChatApiConfig chat_api; // can be further reduced to BackendConfig
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    bool breves = false;
    int parallelism = 1;
};

std::expected<Config, int> get_config(int argc, char* argv[]);
