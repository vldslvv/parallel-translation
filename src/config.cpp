#include "config.hpp"

#include <toml++/toml.hpp>
#include <cstdlib>
#include <filesystem>

static std::filesystem::path config_file_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = xdg
        ? std::filesystem::path{xdg}
        : std::filesystem::path{std::getenv("HOME")} / ".config";
    return base / "parallel-translation" / "config.toml";
}

static void apply_env(Config& cfg) {
    if (const char* v = std::getenv("PT_OLLAMA_HOST"))  cfg.ollama_host  = v;
    if (const char* v = std::getenv("PT_OLLAMA_MODEL")) cfg.ollama_model = v;
    if (const char* v = std::getenv("PT_SOURCE_LANG"))  cfg.source_lang  = v;
    if (const char* v = std::getenv("PT_TARGET_LANG"))  cfg.target_lang  = v;
}

Config load_config() {
    Config cfg;

    auto path = config_file_path();
    if (std::filesystem::exists(path)) {
        auto tbl = toml::parse_file(path.string());
        if (auto v = tbl["ollama"]["host"].value<std::string>())             cfg.ollama_host  = *v;
        if (auto v = tbl["ollama"]["model"].value<std::string>())            cfg.ollama_model = *v;
        if (auto v = tbl["translation"]["source_lang"].value<std::string>()) cfg.source_lang  = *v;
        if (auto v = tbl["translation"]["target_lang"].value<std::string>()) cfg.target_lang  = *v;
    }

    apply_env(cfg);
    return cfg;
}
