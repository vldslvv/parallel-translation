#include "config.hpp"

#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>

#include <spdlog/spdlog.h>

static std::filesystem::path config_file_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = (xdg != nullptr && *xdg != '\0')
                                     ? std::filesystem::path{xdg}
                                     : std::filesystem::path{std::getenv("HOME")} / ".config";
    return base / "parallel-translation" / "config.toml";
}

static void apply_env(Config& cfg) {
    if (const char* v = std::getenv("PT_OLLAMA_HOST"))
        cfg.ollama_host = v;
    if (const char* v = std::getenv("PT_OLLAMA_MODEL"))
        cfg.ollama_model = v;
    if (const char* v = std::getenv("PT_MORPHEUS_DIR"))
        cfg.morpheus_dir = v;
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
        if (auto v = tbl["ollama"]["host"].value<std::string>())
            cfg.ollama_host = *v;
        if (auto v = tbl["ollama"]["model"].value<std::string>())
            cfg.ollama_model = *v;
        if (auto v = tbl["morpheus"]["dir"].value<std::string>())
            cfg.morpheus_dir = *v;
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
    return cfg;
}
