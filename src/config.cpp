#include "config.hpp"

#include <array>
#include <cstddef>
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
};

struct ProviderUserConfig {
    std::string_view provider;
    ChatApiUserConfig config;
};

struct ChatApiRoute {
    std::string_view api_style;
    std::string_view endpoint_path;
};

constexpr std::array<ProviderInfo, 3> providers{{
    {.name = "ollama",
     .default_host = "http://localhost:11434",
     .default_model = "gemma3:27b"},
    {.name = "openrouter",
     .default_host = "https://openrouter.ai",
     .default_model = "google/gemma-4-31b-it"},
    {.name = "opencode",
     .default_host = "https://opencode.ai",
     .default_model = "kimi-k2.6"},
}};

struct UserConfig {
    ReaderConfig reader;
    PostprocessingConfig postprocessing;
    BackendConfig backend;
    WriterConfig writer;
    std::string backend_chat_api_provider = "ollama";
    std::array<ProviderUserConfig, providers.size()> chat_api;
    std::string log_level = "warn";
};

struct CliOverrides {
    std::string chat_provider;
    std::string chat_host;
    std::string chat_model;
    std::string chat_api_key;
    std::string log_level;
};

const ProviderInfo* provider_info(std::string_view provider) {
    for (const auto& info : providers) {
        if (info.name == provider)
            return &info;
    }
    return nullptr;
}

ChatApiRoute chat_api_route(std::string_view provider, std::string_view model) {
    if (provider == "ollama")
        return {.api_style = "ollama-chat", .endpoint_path = "/api/chat"};
    if (provider == "openrouter") {
        if (model.starts_with("anthropic/"))
            return {.api_style = "anthropic-messages", .endpoint_path = "/api/v1/messages"};
        return {.api_style = "openai-chat-completions",
                .endpoint_path = "/api/v1/chat/completions"};
    }
    if (provider == "opencode")
        return {.api_style = "openai-chat-completions",
                .endpoint_path = "/zen/go/v1/chat/completions"};

    throw std::runtime_error{"chat-api: unknown provider route: " + std::string{provider}};
}

ChatApiUserConfig default_chat_api_user_config(const ProviderInfo& provider) {
    return {.host = std::string{provider.default_host},
            .model = std::string{provider.default_model},
            .api_key = {}};
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

ChatApiUserConfig read_provider_table(const toml::node_view<toml::node>& chat_api,
                                      const ProviderInfo& provider) {
    ChatApiUserConfig cfg = default_chat_api_user_config(provider);
    const auto provider_key = std::string{provider.name};

    if (auto v = chat_api[provider_key]["host"].value<std::string>())
        cfg.host = *v;
    if (auto v = chat_api[provider_key]["model"].value<std::string>())
        cfg.model = *v;
    if (auto v = chat_api[provider_key]["api_key"].value<std::string>())
        cfg.api_key = *v;

    return cfg;
}

std::expected<void, int> load_file_config(UserConfig& cfg) {
    const auto path = config_file_path();
    if (!path || !std::filesystem::exists(*path))
        return {};

    try {
        auto tbl = toml::parse_file(path->string());

        if (auto v = tbl["reader"]["path"].value<std::string>())
            cfg.reader.path = *v;
        if (auto v = tbl["writer"]["path"].value<std::string>())
            cfg.writer.path = *v;
        if (auto v = tbl["postprocessing"]["provider"].value<std::string>())
            cfg.postprocessing.provider = *v;
        if (auto v = tbl["postprocessing"]["breves"].value<bool>())
            cfg.postprocessing.breves = *v;
        if (auto v = tbl["backend"]["provider"].value<std::string>())
            cfg.backend.provider = *v;
        if (auto v = tbl["backend"]["source_lang"].value<std::string>())
            cfg.backend.source_lang = *v;
        if (auto v = tbl["backend"]["target_lang"].value<std::string>())
            cfg.backend.target_lang = *v;
        if (auto v = tbl["backend"]["parallelism"].value<int>())
            cfg.backend.parallelism = *v;

        const auto backend_chat_api = tbl["backend"]["chat_api"];
        if (auto v = backend_chat_api["provider"].value<std::string>())
            cfg.backend_chat_api_provider = *v;
        for (const auto& provider : providers) {
            if (auto* entry = provider_user_config(cfg, provider.name))
                entry->config = read_provider_table(backend_chat_api, provider);
        }

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
    auto* selected = provider_user_config(cfg, cfg.backend_chat_api_provider);
    if (selected == nullptr)
        return;

    if (!host.empty())
        selected->config.host = host;
    if (!model.empty())
        selected->config.model = model;
    if (!api_key.empty())
        selected->config.api_key = api_key;
}

std::optional<bool> parse_bool_env(std::string_view value) {
    if (value == "1" || value == "true" || value == "yes" || value == "on")
        return true;
    if (value == "0" || value == "false" || value == "no" || value == "off")
        return false;
    return std::nullopt;
}

void apply_env_config(UserConfig& cfg) {
    if (const char* v = std::getenv("PT_READER_PATH"))
        cfg.reader.path = v;
    if (const char* v = std::getenv("PT_WRITER_PATH"))
        cfg.writer.path = v;
    if (const char* v = std::getenv("PT_BACKEND_PROVIDER"))
        cfg.backend.provider = v;
    if (const char* v = std::getenv("PT_POSTPROCESSOR_PROVIDER"))
        cfg.postprocessing.provider = v;
    if (const char* v = std::getenv("PT_POSTPROCESSOR_BREVES")) {
        if (auto parsed = parse_bool_env(v))
            cfg.postprocessing.breves = *parsed;
        else
            spdlog::warn("PT_POSTPROCESSOR_BREVES='{}' is not a valid boolean, using prior value",
                         v);
    }
    if (const char* v = std::getenv("PT_BACKEND_CHAT_PROVIDER"))
        cfg.backend_chat_api_provider = v;

    apply_chat_api_overrides(cfg, env_value("PT_BACKEND_CHAT_HOST"),
                             env_value("PT_BACKEND_CHAT_MODEL"),
                             env_value("PT_BACKEND_CHAT_API_KEY"));

    if (const char* v = std::getenv("PT_BACKEND_SOURCE_LANG"))
        cfg.backend.source_lang = v;
    if (const char* v = std::getenv("PT_BACKEND_TARGET_LANG"))
        cfg.backend.target_lang = v;
    if (const char* v = std::getenv("PT_BACKEND_PARALLELISM")) {
        try {
            cfg.backend.parallelism = std::stoi(v);
        } catch (...) {
            spdlog::warn("PT_BACKEND_PARALLELISM='{}' is not a valid integer, using prior value",
                         v);
        }
    }
    if (const char* v = std::getenv("PT_LOG_LEVEL"))
        cfg.log_level = v;
}

std::expected<CliOverrides, int> parse_cli_config(int argc, char* argv[], UserConfig& cfg) {
    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    CliOverrides cli;

    app.add_option("--reader-path", cfg.reader.path, "Input file path");
    app.add_option("--writer-path", cfg.writer.path, "Output file path");
    app.add_option("--backend-provider", cfg.backend.provider,
                   "Translation backend provider: chat-api, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocessor-provider", cfg.postprocessing.provider,
                   "Macron postprocessor provider: morpheus, chat-api, none")
        ->capture_default_str();
    app.add_flag("--postprocessor-breves", cfg.postprocessing.breves,
                 "Mark short vowels with a breve (morpheus only)");
    app.add_option("--backend-chat-provider", cli.chat_provider,
                   "Backend Chat API provider: ollama, openrouter, opencode");
    app.add_option("--backend-chat-host", cli.chat_host, "Backend Chat API host URL");
    app.add_option("--backend-chat-model", cli.chat_model, "Backend Chat API model name");
    app.add_option("--backend-chat-api-key", cli.chat_api_key, "Backend Chat API key");
    app.add_option("--backend-source-lang", cfg.backend.source_lang, "Backend source language")
        ->capture_default_str();
    app.add_option("--backend-target-lang", cfg.backend.target_lang, "Backend target language")
        ->capture_default_str();
    app.add_option("--backend-parallelism", cfg.backend.parallelism, "Max concurrent translations")
        ->capture_default_str();
    app.add_option("--log-level", cli.log_level,
                   "Log level: trace/debug/info/warn/error/critical/off");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return std::unexpected(app.exit(e));
    }

    return cli;
}

void apply_cli_overrides(UserConfig& cfg, const CliOverrides& cli) {
    if (!cli.chat_provider.empty())
        cfg.backend_chat_api_provider = cli.chat_provider;
    apply_chat_api_overrides(cfg, cli.chat_host, cli.chat_model, cli.chat_api_key);
    if (!cli.log_level.empty())
        cfg.log_level = cli.log_level;
}

std::expected<ChatApiConfig, int> selected_provider_config(const UserConfig& cfg) {
    const auto* provider = provider_info(cfg.backend_chat_api_provider);
    const auto* selected = provider_user_config(cfg, cfg.backend_chat_api_provider);
    if (provider == nullptr || selected == nullptr) {
        spdlog::error("backend chat-api: unknown provider: {}", cfg.backend_chat_api_provider);
        return std::unexpected(exit_code::usage_error);
    }

    const auto route = chat_api_route(provider->name, selected->config.model);
    return ChatApiConfig{.provider = std::string{provider->name},
                         .host = selected->config.host,
                         .model = selected->config.model,
                         .api_key = selected->config.api_key,
                         .api_style = std::string{route.api_style},
                         .endpoint_path = std::string{route.endpoint_path}};
}

void apply_selected_chat_api_config(UserConfig& cfg, const ChatApiConfig& chat_api) {
    cfg.backend.chat_api = chat_api;
    cfg.postprocessing.chat_api = chat_api;
}

std::expected<std::string, int> reader_format_from_path(const std::string& path) {
    const auto extension = std::filesystem::path{path}.extension().string();
    if (extension == ".txt")
        return "txt";
    if (extension == ".pdf")
        return "pdf";

    spdlog::error("unsupported reader extension: {}", extension);
    return std::unexpected(exit_code::usage_error);
}

std::expected<std::string, int> writer_format_from_path(const std::string& path) {
    const auto extension = std::filesystem::path{path}.extension().string();
    if (extension == ".txt")
        return "txt";
    if (extension == ".pdf")
        return "pdf";

    spdlog::error("unsupported writer extension: {}", extension);
    return std::unexpected(exit_code::usage_error);
}

std::expected<void, int> resolve_pipeline_config(UserConfig& cfg) {
    if (cfg.reader.path.empty()) {
        spdlog::error("reader path is required");
        return std::unexpected(exit_code::usage_error);
    }
    if (cfg.writer.path.empty()) {
        spdlog::error("writer path is required");
        return std::unexpected(exit_code::usage_error);
    }

    auto reader_format = reader_format_from_path(cfg.reader.path);
    if (!reader_format)
        return std::unexpected(reader_format.error());
    cfg.reader.format = std::move(*reader_format);

    auto writer_format = writer_format_from_path(cfg.writer.path);
    if (!writer_format)
        return std::unexpected(writer_format.error());
    cfg.writer.format = std::move(*writer_format);

    if (cfg.backend.parallelism < 1) {
        spdlog::error("backend parallelism must be at least 1");
        return std::unexpected(exit_code::usage_error);
    }

    return {};
}

void log_config(const Config& config) {
    spdlog::debug("config: reader_path={}", config.reader.path);
    spdlog::debug("config: reader_format={}", config.reader.format);
    spdlog::debug("config: postprocessing_provider={}", config.postprocessing.provider);
    spdlog::debug("config: postprocessing_breves={}", config.postprocessing.breves);
    spdlog::debug("config: backend_provider={}", config.backend.provider);
    spdlog::debug("config: backend_chat_api_provider={}", config.backend.chat_api.provider);
    spdlog::debug("config: backend_chat_api_host={}", config.backend.chat_api.host);
    spdlog::debug("config: backend_chat_api_model={}", config.backend.chat_api.model);
    spdlog::debug("config: backend_chat_api_api_key={}",
                  config.backend.chat_api.api_key.empty() ? "(empty)" : "(set)");
    spdlog::debug("config: backend_chat_api_style={}", config.backend.chat_api.api_style);
    spdlog::debug("config: backend_chat_api_endpoint_path={}",
                  config.backend.chat_api.endpoint_path);
    spdlog::debug("config: backend_source_lang={}", config.backend.source_lang);
    spdlog::debug("config: backend_target_lang={}", config.backend.target_lang);
    spdlog::debug("config: backend_parallelism={}", config.backend.parallelism);
    spdlog::debug("config: writer_path={}", config.writer.path);
    spdlog::debug("config: writer_format={}", config.writer.format);
    spdlog::debug("config: log_level={}", config.log_level);
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

    if (auto resolved = resolve_pipeline_config(user_config); !resolved)
        return std::unexpected(resolved.error());

    auto chat_api = selected_provider_config(user_config);
    if (!chat_api)
        return std::unexpected(chat_api.error());

    apply_selected_chat_api_config(user_config, *chat_api);

    Config config{.reader = std::move(user_config.reader),
                  .postprocessing = std::move(user_config.postprocessing),
                  .backend = std::move(user_config.backend),
                  .writer = std::move(user_config.writer),
                  .log_level = std::move(user_config.log_level)};

    spdlog::set_level(spdlog::level::from_str(config.log_level));
    log_config(config);

    return config;
}
