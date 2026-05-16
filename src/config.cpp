#include "config.hpp"

#include <array>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"

struct ChatApiUserConfig {
    std::string host;
    std::string model;
    std::string api_key;
};

struct UserConfig {
    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    std::string chat_api_provider = "ollama";
    ChatApiUserConfig ollama;
    ChatApiUserConfig openrouter;
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    bool breves = false;
    int parallelism = 1;
};

static std::filesystem::path config_file_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = (xdg != nullptr && *xdg != '\0')
                                     ? std::filesystem::path{xdg}
                                     : std::filesystem::path{std::getenv("HOME")} / ".config";
    return base / "parallel-translation" / "config.toml";
}

static ChatApiUserConfig default_chat_api_user_config(std::string_view provider) {
    if (provider == "ollama")
        return {.host = "http://localhost:11434", .model = "gemma3:27b"};
    if (provider == "openrouter")
        return {.host = "https://openrouter.ai", .model = "google/gemma-4-31b-it"};
    throw std::runtime_error{"chat-api: unknown provider: " + std::string{provider}};
}

static ChatApiUserConfig get_provider_table(const toml::table& tbl, std::string_view provider) {
    ChatApiUserConfig cfg = default_chat_api_user_config(provider);
    const auto provider_key = std::string{provider};

    if (auto v = tbl[provider_key]["host"].value<std::string>())
        cfg.host = *v;
    if (auto v = tbl[provider_key]["model"].value<std::string>())
        cfg.model = *v;
    if (auto v = tbl[provider_key]["api_key"].value<std::string>())
        cfg.api_key = *v;

    return cfg;
}

static ChatApiUserConfig get_chat_api_env_config(ChatApiUserConfig cfg) {
    if (const char* v = std::getenv("PT_CHAT_HOST"))
        cfg.host = v;
    if (const char* v = std::getenv("PT_CHAT_MODEL"))
        cfg.model = v;
    if (const char* v = std::getenv("PT_CHAT_API_KEY"))
        cfg.api_key = v;

    return cfg;
}

static UserConfig get_selected_chat_env_config(UserConfig cfg) {
    if (cfg.chat_api_provider == "ollama") {
        cfg.ollama = get_chat_api_env_config(std::move(cfg.ollama));
        return cfg;
    }
    if (cfg.chat_api_provider == "openrouter") {
        cfg.openrouter = get_chat_api_env_config(std::move(cfg.openrouter));
        return cfg;
    }

    return cfg;
}

struct ChatApiRoute {
    std::string_view api_style;
    std::string_view endpoint_path;
};

struct ChatApiModelRoute {
    std::string_view provider;
    std::string_view model_prefix;
    ChatApiRoute route;
};

static std::optional<ChatApiRoute> model_chat_api_route(std::string_view provider,
                                                        std::string_view model) {
    static constexpr std::array<ChatApiModelRoute, 0> routes{};

    for (const auto& route : routes) {
        if (route.provider == provider && model.starts_with(route.model_prefix))
            return route.route;
    }
    return std::nullopt;
}

static ChatApiRoute default_chat_api_route(std::string_view provider) {
    if (provider == "ollama")
        return {.api_style = "ollama-chat", .endpoint_path = "/api/chat"};
    if (provider == "openrouter")
        return {.api_style = "openai-chat-completions",
                .endpoint_path = "/api/v1/chat/completions"};
    throw std::runtime_error{"chat-api: unknown provider: " + std::string{provider}};
}

static ChatApiConfig with_chat_api_route(ChatApiConfig cfg) {
    auto route = model_chat_api_route(cfg.provider, cfg.model);
    if (!route)
        route = default_chat_api_route(cfg.provider);
    cfg.api_style = route->api_style;
    cfg.endpoint_path = route->endpoint_path;
    return cfg;
}

static UserConfig get_env_config(UserConfig cfg) {
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

static UserConfig load_config() {
    UserConfig cfg;
    cfg.ollama = default_chat_api_user_config("ollama");
    cfg.openrouter = default_chat_api_user_config("openrouter");

    auto path = config_file_path();
    if (std::filesystem::exists(path)) {
        cfg.config_file = path.string();
        auto tbl = toml::parse_file(path.string());
        if (auto v = tbl["chat_api"]["provider"].value<std::string>())
            cfg.chat_api_provider = *v;
        cfg.ollama = get_provider_table(tbl, "ollama");
        cfg.openrouter = get_provider_table(tbl, "openrouter");
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

static void apply_chat_api_cli_overrides(UserConfig& cfg, const std::string& model,
                                         const std::string& host, const std::string& api_key) {
    auto apply = [&](ChatApiUserConfig& user_cfg) {
        if (!model.empty())
            user_cfg.model = model;
        if (!host.empty())
            user_cfg.host = host;
        if (!api_key.empty())
            user_cfg.api_key = api_key;
    };

    if (cfg.chat_api_provider == "ollama") {
        apply(cfg.ollama);
        return;
    }
    if (cfg.chat_api_provider == "openrouter") {
        apply(cfg.openrouter);
    }
}

static ChatApiConfig selected_chat_api_config(const UserConfig& cfg) {
    if (cfg.chat_api_provider == "ollama")
        return with_chat_api_route(ChatApiConfig{.provider = "ollama",
                                                 .host = cfg.ollama.host,
                                                 .model = cfg.ollama.model,
                                                 .api_key = cfg.ollama.api_key});
    if (cfg.chat_api_provider == "openrouter")
        return with_chat_api_route(ChatApiConfig{.provider = "openrouter",
                                                 .host = cfg.openrouter.host,
                                                 .model = cfg.openrouter.model,
                                                 .api_key = cfg.openrouter.api_key});
    throw std::runtime_error{"chat-api: unknown provider: " + cfg.chat_api_provider};
}

std::expected<Config, int> get_config(int argc, char* argv[]) {
    UserConfig user_config = load_config();

    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    std::string provider;
    std::string model;
    std::string host;
    std::string api_key;
    std::string log_level;

    app.add_option("--input,-i", user_config.input_file, "Input file path")->required();
    app.add_option("--output,-o", user_config.output_file, "Output file path")->required();
    app.add_option("--backend", user_config.backend, "Translation backend: chat-api, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocess", user_config.postprocess,
                   "Macron postprocessor: morpheus, chat-api, none")
        ->capture_default_str();
    app.add_flag("--breves", user_config.breves, "Mark short vowels with a breve (morpheus only)");
    app.add_option("--chat-provider", provider, "Chat API provider: ollama, openrouter");
    app.add_option("--chat-host", host, "Chat API host URL (overrides config)");
    app.add_option("--chat-model", model, "Chat API model name (overrides config)");
    app.add_option("--chat-api-key", api_key, "Chat API key (overrides config)");
    app.add_option("--log-level", log_level, "Log level: trace/debug/info/warn/error/critical/off");
    app.add_option("--parallelism", user_config.parallelism, "Max concurrent translations")
        ->capture_default_str();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return std::unexpected(app.exit(e));
    }

    if (!provider.empty())
        user_config.chat_api_provider = provider;
    apply_chat_api_cli_overrides(user_config, model, host, api_key);

    ChatApiConfig chat_api;
    try {
        chat_api = selected_chat_api_config(user_config);
    } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        return std::unexpected(exit_code::usage_error);
    }

    if (!log_level.empty())
        user_config.log_level = log_level;

    Config config{.input_file = user_config.input_file,
                  .output_file = user_config.output_file,
                  .backend = user_config.backend,
                  .postprocess = user_config.postprocess,
                  .chat_api = std::move(chat_api),
                  .source_lang = user_config.source_lang,
                  .target_lang = user_config.target_lang,
                  .log_level = user_config.log_level,
                  .config_file = user_config.config_file,
                  .breves = user_config.breves,
                  .parallelism = user_config.parallelism};

    spdlog::set_level(spdlog::level::from_str(config.log_level));
    spdlog::debug("config: file={}", config.config_file.empty() ? "(none)" : config.config_file);
    spdlog::debug("config: chat_api_provider={}", config.chat_api.provider);
    spdlog::debug("config: chat_api_host={}", config.chat_api.host);
    spdlog::debug("config: chat_api_model={}", config.chat_api.model);
    spdlog::debug("config: chat_api_api_key={}",
                  config.chat_api.api_key.empty() ? "(empty)" : "(set)");
    spdlog::debug("config: chat_api_style={}", config.chat_api.api_style);
    spdlog::debug("config: chat_api_endpoint_path={}", config.chat_api.endpoint_path);
    spdlog::debug("config: source_lang={}", config.source_lang);
    spdlog::debug("config: target_lang={}", config.target_lang);
    spdlog::debug("config: log_level={}", config.log_level);
    spdlog::debug("config: parallelism={}", config.parallelism);

    return config;
}
