#include "config.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <toml++/toml.hpp>
#include <utility>

#include <spdlog/spdlog.h>

static std::filesystem::path config_file_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = (xdg != nullptr && *xdg != '\0')
                                     ? std::filesystem::path{xdg}
                                     : std::filesystem::path{std::getenv("HOME")} / ".config";
    return base / "parallel-translation" / "config.toml";
}

static OllamaConfig get_ollama_table(const toml::table& tbl) {
    OllamaConfig cfg;

    if (auto v = tbl["ollama"]["host"].value<std::string>())
        cfg.host = *v;
    if (auto v = tbl["ollama"]["model"].value<std::string>())
        cfg.model = *v;
    if (auto v = tbl["ollama"]["api_key"].value<std::string>())
        cfg.api_key = *v;

    return cfg;
}

static OpenRouterConfig get_openrouter_table(const toml::table& tbl) {
    OpenRouterConfig cfg;

    if (auto v = tbl["openrouter"]["host"].value<std::string>())
        cfg.host = *v;
    if (auto v = tbl["openrouter"]["model"].value<std::string>())
        cfg.model = *v;
    if (auto v = tbl["openrouter"]["api_key"].value<std::string>())
        cfg.api_key = *v;

    return cfg;
}

static OllamaConfig get_ollama_env_config(OllamaConfig cfg) {
    if (const char* v = std::getenv("PT_CHAT_HOST"))
        cfg.host = v;
    if (const char* v = std::getenv("PT_CHAT_MODEL"))
        cfg.model = v;
    if (const char* v = std::getenv("PT_CHAT_API_KEY"))
        cfg.api_key = v;

    return cfg;
}

static OpenRouterConfig get_openrouter_env_config(OpenRouterConfig cfg) {
    if (const char* v = std::getenv("PT_CHAT_HOST"))
        cfg.host = v;
    if (const char* v = std::getenv("PT_CHAT_MODEL"))
        cfg.model = v;
    if (const char* v = std::getenv("PT_CHAT_API_KEY"))
        cfg.api_key = v;

    return cfg;
}

static Config get_selected_chat_env_config(Config cfg) {
    if (cfg.chat_api_provider == "ollama") {
        cfg.ollama = get_ollama_env_config(std::move(cfg.ollama));
        return cfg;
    }
    if (cfg.chat_api_provider == "openrouter") {
        cfg.openrouter = get_openrouter_env_config(std::move(cfg.openrouter));
        return cfg;
    }

    return cfg;
}

static Config get_env_config(Config cfg) {
    if (const char* v = std::getenv("PT_CHAT_PROVIDER"))
        cfg.chat_api_provider = v;
    cfg = get_selected_chat_env_config(std::move(cfg));
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

    return cfg;
}

Config load_config() {
    Config cfg;

    auto path = config_file_path();
    if (std::filesystem::exists(path)) {
        cfg.config_file = path.string();
        auto tbl = toml::parse_file(path.string());
        if (auto v = tbl["chat_api"]["provider"].value<std::string>())
            cfg.chat_api_provider = *v;
        cfg.ollama = get_ollama_table(tbl);
        cfg.openrouter = get_openrouter_table(tbl);
        if (auto v = tbl["translation"]["source_lang"].value<std::string>())
            cfg.source_lang = *v;
        if (auto v = tbl["translation"]["target_lang"].value<std::string>())
            cfg.target_lang = *v;
        if (auto v = tbl["log"]["level"].value<std::string>())
            cfg.log_level = *v;
        if (auto v = tbl["translation"]["parallelism"].value<int>())
            cfg.parallelism = *v;
    }

    return get_env_config(std::move(cfg));
}

ChatApiConfig selected_chat_api_config(const Config& cfg) {
    if (cfg.chat_api_provider == "ollama")
        return ChatApiConfig{.provider = "ollama",
                             .host = cfg.ollama.host,
                             .model = cfg.ollama.model,
                             .api_key = cfg.ollama.api_key};
    if (cfg.chat_api_provider == "openrouter")
        return ChatApiConfig{.provider = "openrouter",
                             .host = cfg.openrouter.host,
                             .model = cfg.openrouter.model,
                             .api_key = cfg.openrouter.api_key};
    throw std::runtime_error{"chat-api: unknown provider: " + cfg.chat_api_provider};
}
