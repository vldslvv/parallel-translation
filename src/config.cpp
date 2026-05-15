#include "config.hpp"

#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>

#include <spdlog/spdlog.h>

static std::string default_chat_api_host(const std::string& provider) {
    if (provider == "openrouter")
        return "https://openrouter.ai";
    return "http://localhost:11434";
}

static std::filesystem::path config_file_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = (xdg != nullptr && *xdg != '\0')
                                     ? std::filesystem::path{xdg}
                                     : std::filesystem::path{std::getenv("HOME")} / ".config";
    return base / "parallel-translation" / "config.toml";
}

static void apply_env(Config& cfg) {
    if (const char* v = std::getenv("PT_CHAT_PROVIDER"))
        cfg.chat_api.provider = v;
    if (const char* v = std::getenv("PT_CHAT_HOST"))
        cfg.chat_api.host = v;
    if (const char* v = std::getenv("PT_CHAT_MODEL"))
        cfg.chat_api.model = v;
    if (const char* v = std::getenv("PT_CHAT_API_KEY"))
        cfg.chat_api.api_key = v;
    if (const char* v = std::getenv("PT_SOURCE_LANG"))
        cfg.source_lang = v;
    if (const char* v = std::getenv("PT_TARGET_LANG"))
        cfg.target_lang = v;
    if (const char* v = std::getenv("PT_LOG_LEVEL"))
        cfg.log_level = v;
    if (const char* v = std::getenv("PT_PARALLELISM")) {
        try {
            cfg.parallelism = std::stoi(v);
        } catch (...) {
            spdlog::warn("PT_PARALLELISM='{}' is not a valid integer, using default", v);
        }
    }
}

Config load_config() {
    Config cfg;

    auto path = config_file_path();
    if (std::filesystem::exists(path)) {
        cfg.config_file = path.string();
        auto tbl = toml::parse_file(path.string());
        if (auto v = tbl["chat_api"]["provider"].value<std::string>())
            cfg.chat_api.provider = *v;
        if (auto v = tbl["chat_api"]["host"].value<std::string>())
            cfg.chat_api.host = *v;
        if (auto v = tbl["chat_api"]["model"].value<std::string>())
            cfg.chat_api.model = *v;
        if (auto v = tbl["chat_api"]["api_key"].value<std::string>())
            cfg.chat_api.api_key = *v;
        if (auto v = tbl["translation"]["source_lang"].value<std::string>())
            cfg.source_lang = *v;
        if (auto v = tbl["translation"]["target_lang"].value<std::string>())
            cfg.target_lang = *v;
        if (auto v = tbl["log"]["level"].value<std::string>())
            cfg.log_level = *v;
        if (auto v = tbl["translation"]["parallelism"].value<int>())
            cfg.parallelism = *v;
    }

    apply_env(cfg);
    if (cfg.chat_api.host.empty())
        cfg.chat_api.host = default_chat_api_host(cfg.chat_api.provider);
    return cfg;
}
