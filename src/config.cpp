#include "config.hpp"

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <stdexcept>
#include <toml++/toml.hpp>
#include <utility>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"

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

static void apply_chat_api_cli_overrides(Config& cfg, const std::string& model,
                                         const std::string& host, const std::string& api_key) {
    if (cfg.chat_api_provider == "ollama") {
        if (!model.empty())
            cfg.ollama.model = model;
        if (!host.empty())
            cfg.ollama.host = host;
        if (!api_key.empty())
            cfg.ollama.api_key = api_key;
        return;
    }
    if (cfg.chat_api_provider == "openrouter") {
        if (!model.empty())
            cfg.openrouter.model = model;
        if (!host.empty())
            cfg.openrouter.host = host;
        if (!api_key.empty())
            cfg.openrouter.api_key = api_key;
    }
}

std::expected<Config, int> get_config(int argc, char* argv[]) {
    Config config = load_config();

    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    std::string provider;
    std::string model;
    std::string host;
    std::string api_key;
    std::string log_level;

    app.add_option("--input,-i", config.input_file, "Input file path")->required();
    app.add_option("--output,-o", config.output_file, "Output file path")->required();
    app.add_option("--backend", config.backend, "Translation backend: chat-api, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocess", config.postprocess,
                   "Macron postprocessor: morpheus, chat-api, none")
        ->capture_default_str();
    app.add_flag("--breves", config.breves, "Mark short vowels with a breve (morpheus only)");
    app.add_option("--chat-provider", provider, "Chat API provider: ollama, openrouter");
    app.add_option("--chat-host", host, "Chat API host URL (overrides config)");
    app.add_option("--chat-model", model, "Chat API model name (overrides config)");
    app.add_option("--chat-api-key", api_key, "Chat API key (overrides config)");
    app.add_option("--log-level", log_level, "Log level: trace/debug/info/warn/error/critical/off");
    app.add_option("--parallelism", config.parallelism, "Max concurrent translations")
        ->capture_default_str();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return std::unexpected(app.exit(e));
    }

    if (!provider.empty())
        config.chat_api_provider = provider;
    apply_chat_api_cli_overrides(config, model, host, api_key);

    ChatApiConfig chat_api;
    try {
        chat_api = selected_chat_api_config(config);
    } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        return std::unexpected(exit_code::usage_error);
    }

    if (!log_level.empty())
        config.log_level = log_level;
    spdlog::set_level(spdlog::level::from_str(config.log_level));
    spdlog::debug("config: file={}", config.config_file.empty() ? "(none)" : config.config_file);
    spdlog::debug("config: chat_api_provider={}", config.chat_api_provider);
    spdlog::debug("config: chat_api_host={}", chat_api.host);
    spdlog::debug("config: chat_api_model={}", chat_api.model);
    spdlog::debug("config: chat_api_api_key={}", chat_api.api_key.empty() ? "(empty)" : "(set)");
    spdlog::debug("config: source_lang={}", config.source_lang);
    spdlog::debug("config: target_lang={}", config.target_lang);
    spdlog::debug("config: log_level={}", config.log_level);
    spdlog::debug("config: parallelism={}", config.parallelism);

    return config;
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
