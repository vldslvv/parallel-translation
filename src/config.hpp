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

struct ReaderConfig {
    std::string path;
    std::string format;
};

struct PostprocessingConfig {
    std::string provider = "morpheus";
    bool breves = false;
    ChatApiConfig chat_api;
};

struct BackendConfig {
    std::string provider = "chat-api";
    std::string source_lang = "la";
    std::string target_lang = "en";
    int parallelism = 1;
    ChatApiConfig chat_api;
};

struct WriterConfig {
    std::string path;
    std::string format;
};

struct Config {
    ReaderConfig reader;
    PostprocessingConfig postprocessing;
    BackendConfig backend;
    WriterConfig writer;
    std::string log_level = "warn";
};

std::expected<Config, int> get_config(int argc, char* argv[]);
