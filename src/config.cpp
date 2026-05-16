#include "config.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"

namespace {

struct ChatApiUserConfig {
    std::string host;
    std::string model;
    std::string api_key;
};

struct ProviderInfo {
    std::string_view name;
    std::string_view default_host;
    std::string_view default_model;
    std::string_view api_style;
    std::string_view endpoint_path;
};

struct ProviderUserConfig {
    std::string_view provider;
    ChatApiUserConfig config;
};

struct UserConfig {
    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    std::string chat_api_provider = "ollama";
    std::array<ProviderUserConfig, 2> chat_api;
    std::string source_lang = "la";
    std::string target_lang = "en";
    std::string log_level = "warn";
    std::string config_file; // path used, empty if none found
    bool breves = false;
    int parallelism = 1;
};

struct CliOverrides {
    std::string chat_provider;
    std::string chat_host;
    std::string chat_model;
    std::string chat_api_key;
    std::string log_level;
};

constexpr std::array<ProviderInfo, 2> providers{{
    {.name = "ollama",
     .default_host = "http://localhost:11434",
     .default_model = "gemma3:27b",
     .api_style = "ollama-chat",
     .endpoint_path = "/api/chat"},
    {.name = "openrouter",
     .default_host = "https://openrouter.ai",
     .default_model = "google/gemma-4-31b-it",
     .api_style = "openai-chat-completions",
     .endpoint_path = "/api/v1/chat/completions"},
}};

const ProviderInfo* provider_info(std::string_view provider) {
    for (const auto& info : providers) {
        if (info.name == provider)
            return &info;
    }
    return nullptr;
}

ChatApiUserConfig default_chat_api_user_config(const ProviderInfo& provider) {
    return {.host = std::string{provider.default_host},
            .model = std::string{provider.default_model}};
}

UserConfig make_default_user_config() {
    UserConfig cfg;
    for (std::size_t i = 0; i < providers.size(); ++i) {
        cfg.chat_api[i] = {.provider = providers[i].name,
                           .config = default_chat_api_user_config(providers[i])};
    }
    return cfg;
}

ProviderUserConfig* provider_user_config(UserConfig& cfg, std::string_view provider) {
    for (auto& entry : cfg.chat_api) {
        if (entry.provider == provider)
            return &entry;
    }
    return nullptr;
}

const ProviderUserConfig* provider_user_config(const UserConfig& cfg, std::string_view provider) {
    for (const auto& entry : cfg.chat_api) {
        if (entry.provider == provider)
            return &entry;
    }
    return nullptr;
}

std::string_view env_value(const char* name) {
    if (const char* value = std::getenv(name))
        return value;
    return {};
}

std::optional<std::filesystem::path> config_file_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0')
        return std::filesystem::path{xdg} / "parallel-translation" / "config.toml";

    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
        return std::filesystem::path{home} / ".config" / "parallel-translation" / "config.toml";

    return std::nullopt;
}

ChatApiUserConfig read_provider_table(const toml::table& tbl, const ProviderInfo& provider) {
    ChatApiUserConfig cfg = default_chat_api_user_config(provider);
    const auto provider_key = std::string{provider.name};

    if (auto v = tbl[provider_key]["host"].value<std::string>())
        cfg.host = *v;
    if (auto v = tbl[provider_key]["model"].value<std::string>())
        cfg.model = *v;
    if (auto v = tbl[provider_key]["api_key"].value<std::string>())
        cfg.api_key = *v;

    return cfg;
}

bool has_legacy_chat_api_fields(const toml::table& tbl) {
    return tbl["chat_api"]["host"].value<std::string>().has_value() ||
           tbl["chat_api"]["model"].value<std::string>().has_value() ||
           tbl["chat_api"]["api_key"].value<std::string>().has_value();
}

std::expected<void, int> load_file_config(UserConfig& cfg) {
    const auto path = config_file_path();
    if (!path || !std::filesystem::exists(*path))
        return {};

    try {
        auto tbl = toml::parse_file(path->string());
        if (has_legacy_chat_api_fields(tbl)) {
            spdlog::error("legacy [chat_api] host/model/api_key fields are not supported");
            return std::unexpected(exit_code::usage_error);
        }

        cfg.config_file = path->string();
        if (auto v = tbl["chat_api"]["provider"].value<std::string>())
            cfg.chat_api_provider = *v;
        for (const auto& provider : providers) {
            if (auto* entry = provider_user_config(cfg, provider.name))
                entry->config = read_provider_table(tbl, provider);
        }
        if (auto v = tbl["translation"]["source_lang"].value<std::string>())
            cfg.source_lang = *v;
        if (auto v = tbl["translation"]["target_lang"].value<std::string>())
            cfg.target_lang = *v;
        if (auto v = tbl["translation"]["parallelism"].value<int>())
            cfg.parallelism = *v;
        if (auto v = tbl["log"]["level"].value<std::string>())
            cfg.log_level = *v;
    } catch (const toml::parse_error& e) {
        spdlog::error("failed to parse config file {}: {}", path->string(), e.what());
        return std::unexpected(exit_code::usage_error);
    }

    return {};
}

void apply_chat_api_overrides(UserConfig& cfg, std::string_view host, std::string_view model,
                              std::string_view api_key) {
    auto* selected = provider_user_config(cfg, cfg.chat_api_provider);
    if (selected == nullptr)
        return;

    if (!host.empty())
        selected->config.host = host;
    if (!model.empty())
        selected->config.model = model;
    if (!api_key.empty())
        selected->config.api_key = api_key;
}

void apply_env_config(UserConfig& cfg) {
    if (const char* v = std::getenv("PT_CHAT_PROVIDER"))
        cfg.chat_api_provider = v;

    apply_chat_api_overrides(cfg, env_value("PT_CHAT_HOST"), env_value("PT_CHAT_MODEL"),
                             env_value("PT_CHAT_API_KEY"));

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
            spdlog::warn("PT_PARALLELISM='{}' is not a valid integer, using prior value", v);
        }
    }
}

std::expected<CliOverrides, int> parse_cli_config(int argc, char* argv[], UserConfig& cfg) {
    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    CliOverrides cli;

    app.add_option("--input,-i", cfg.input_file, "Input file path")->required();
    app.add_option("--output,-o", cfg.output_file, "Output file path")->required();
    app.add_option("--backend", cfg.backend, "Translation backend: chat-api, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocess", cfg.postprocess,
                   "Macron postprocessor: morpheus, chat-api, none")
        ->capture_default_str();
    app.add_flag("--breves", cfg.breves, "Mark short vowels with a breve (morpheus only)");
    app.add_option("--chat-provider", cli.chat_provider, "Chat API provider: ollama, openrouter");
    app.add_option("--chat-host", cli.chat_host, "Chat API host URL (overrides config)");
    app.add_option("--chat-model", cli.chat_model, "Chat API model name (overrides config)");
    app.add_option("--chat-api-key", cli.chat_api_key, "Chat API key (overrides config)");
    app.add_option("--log-level", cli.log_level,
                   "Log level: trace/debug/info/warn/error/critical/off");
    app.add_option("--parallelism", cfg.parallelism, "Max concurrent translations")
        ->capture_default_str();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return std::unexpected(app.exit(e));
    }

    return cli;
}

void apply_cli_overrides(UserConfig& cfg, const CliOverrides& cli) {
    if (!cli.chat_provider.empty())
        cfg.chat_api_provider = cli.chat_provider;
    apply_chat_api_overrides(cfg, cli.chat_host, cli.chat_model, cli.chat_api_key);
    if (!cli.log_level.empty())
        cfg.log_level = cli.log_level;
}

std::expected<ChatApiConfig, int> selected_provider_config(const UserConfig& cfg) {
    const auto* provider = provider_info(cfg.chat_api_provider);
    const auto* selected = provider_user_config(cfg, cfg.chat_api_provider);
    if (provider == nullptr || selected == nullptr) {
        spdlog::error("chat-api: unknown provider: {}", cfg.chat_api_provider);
        return std::unexpected(exit_code::usage_error);
    }

    return ChatApiConfig{.provider = std::string{provider->name},
                         .host = selected->config.host,
                         .model = selected->config.model,
                         .api_key = selected->config.api_key,
                         .api_style = std::string{provider->api_style},
                         .endpoint_path = std::string{provider->endpoint_path}};
}

void log_config(const Config& config) {
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
}

} // namespace

std::expected<Config, int> get_config(int argc, char* argv[]) {
    UserConfig user_config = make_default_user_config();

    if (auto loaded = load_file_config(user_config); !loaded)
        return std::unexpected(loaded.error());
    apply_env_config(user_config);

    auto cli = parse_cli_config(argc, argv, user_config);
    if (!cli)
        return std::unexpected(cli.error());
    apply_cli_overrides(user_config, *cli);

    auto chat_api = selected_provider_config(user_config);
    if (!chat_api)
        return std::unexpected(chat_api.error());

    Config config{.input_file = std::move(user_config.input_file),
                  .output_file = std::move(user_config.output_file),
                  .backend = std::move(user_config.backend),
                  .postprocess = std::move(user_config.postprocess),
                  .chat_api = std::move(*chat_api),
                  .source_lang = std::move(user_config.source_lang),
                  .target_lang = std::move(user_config.target_lang),
                  .log_level = std::move(user_config.log_level),
                  .config_file = std::move(user_config.config_file),
                  .breves = user_config.breves,
                  .parallelism = user_config.parallelism};

    if (config.parallelism < 1) {
        spdlog::error("parallelism must be at least 1");
        return std::unexpected(exit_code::usage_error);
    }

    spdlog::set_level(spdlog::level::from_str(config.log_level));
    log_config(config);

    return config;
}
