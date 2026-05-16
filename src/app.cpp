#include "app.hpp"

#include <chrono>

#include <deque>
#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <semaphore>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"
#include "common/scope_exit.hpp"
#include "config.hpp"
#include "formatters/formatter.hpp"
#include "readers/reader.hpp"
#include "translators/morpheus.hpp"
#include "translators/ollama.hpp"
#include "translators/translator.hpp"
#include "writers/formatted_writer.hpp"
#include "writers/pdf.hpp"
#include "writers/writer.hpp"

// TODO: merge with config.cpp, or restructure
struct AppConfig {
    Config config;
    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    bool breves = false;
    int parallelism = 1;
};

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

static std::expected<AppConfig, int> get_config(int argc, char* argv[]) {
    Config config = load_config();

    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    std::string input_file;
    std::string output_file;
    std::string backend = "chat-api";
    std::string postprocess = "morpheus";
    bool breves = false;
    int parallelism = config.parallelism;
    std::string provider;
    std::string model;
    std::string host;
    std::string api_key;
    std::string log_level;

    app.add_option("--input,-i", input_file, "Input file path")->required();
    app.add_option("--output,-o", output_file, "Output file path")->required();
    app.add_option("--backend", backend, "Translation backend: chat-api, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocess", postprocess, "Macron postprocessor: morpheus, chat-api, none")
        ->capture_default_str();
    app.add_flag("--breves", breves, "Mark short vowels with a breve (morpheus only)");
    app.add_option("--chat-provider", provider, "Chat API provider: ollama, openrouter");
    app.add_option("--chat-host", host, "Chat API host URL (overrides config)");
    app.add_option("--chat-model", model, "Chat API model name (overrides config)");
    app.add_option("--chat-api-key", api_key, "Chat API key (overrides config)");
    app.add_option("--log-level", log_level, "Log level: trace/debug/info/warn/error/critical/off");
    app.add_option("--parallelism", parallelism, "Max concurrent translations")->capture_default_str();

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
    spdlog::debug("config: parallelism={}", parallelism);

    return AppConfig{
        .config = std::move(config),
        .input_file = input_file,
        .output_file = output_file,
        .backend = backend,
        .postprocess = postprocess,
        .breves = breves,
        .parallelism = parallelism,
    };
}

static std::expected<Reader, int> get_reader(const std::string& input_file) {
    std::string extension = std::filesystem::path{input_file}.extension();
    if (extension == ".txt") {
        return txt_reader;
    }
    if (extension == ".pdf") {
        return pdf_reader;
    }

    spdlog::error("unsupported extension: {}", extension);
    return std::unexpected(exit_code::usage_error);
}

static std::expected<Translator, int> get_translator(const std::string& backend,
                                                     const Config& cfg) {
    if (backend == "stub") {
        return stub_translator;
    }
    if (backend == "pass") {
        return pass_translator;
    }
    if (backend == "chat-api") {
        try {
            return make_chat_api_latin_to_english_translator(selected_chat_api_config(cfg));
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return std::unexpected(exit_code::usage_error);
        }
    }

    spdlog::error("unknown backend: {}", backend);
    return std::unexpected(exit_code::usage_error);
}

static int translate_file(const Reader& read, const Translator& translate,
                          const Translator& read_postprocess, FormattedWriter& formatted_write,
                          std::string_view input, int parallelism) {
    constexpr int sem_max = 64;
    if (parallelism > sem_max) {
        spdlog::error("parallelism {} exceeds maximum {}", parallelism, sem_max);
        return exit_code::usage_error;
    }
    std::counting_semaphore<sem_max> sem{parallelism};
    std::deque<std::pair<std::string, std::future<std::string>>> work;

    auto flush = [&work, &formatted_write, &read_postprocess]() -> int {
        while (!work.empty()) {
            auto& [original, fut] = work.front();
            if (fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                break;

            std::string translation;
            try {
                translation = fut.get();
            } catch (const std::exception& e) {
                spdlog::error("translation failed: {}", e.what());
                return exit_code::runtime_error;
            }
            // Postprocessed original is useful when Latin text doesn't have
            // macrons, and user wants to insert them. Since it's likely
            // that LLMs are used, a postprocessed original should not be translated
            // to not introduce more noise into the input
            auto original_postprocessed = read_postprocess(original);
            spdlog::debug("postprocessed: {}", original_postprocessed);

            if (auto rc = formatted_write(original_postprocessed, translation); rc != 0)
                return rc;
            work.pop_front();
        }
        return 0;
    };

    // Dispatch: one async task per sentence, limited by semaphore
    for (const auto& item : read(input)) {
        if (!item) {
            spdlog::error("{}", item.error());
            return exit_code::input_error;
        }
        spdlog::debug("{}", *item);
        sem.acquire();
        if (auto rc = flush(); rc != 0)
            return rc;

        // Sentence copies item value so it's retained after outer loop advances
        auto fn = [&sem, &translate, sentence = *item]() -> std::string {
            // Releases semaphore regardless of execution outcome
            ScopeExit guard{[&sem] { sem.release(); }};
            return translate(sentence);
        };
        auto fut = std::async(std::launch::async, fn);
        work.emplace_back(*item, std::move(fut));
    }

    // Drain remaining
    for (auto& [original, fut] : work) {
        std::string translation;
        try {
            translation = fut.get();
        } catch (const std::exception& e) {
            spdlog::error("translation failed: {}", e.what());
            return exit_code::runtime_error;
        }

        auto original_postprocessed = read_postprocess(original);
        spdlog::debug("postprocessed: {}", original_postprocessed);

        if (auto rc = formatted_write(original_postprocessed, translation); rc != 0)
            return rc;
    }
    return 0;
}

int run(int argc, char* argv[]) {
    auto parsed = get_config(argc, argv);
    if (!parsed) {
        return parsed.error();
    }
    AppConfig& app_config = parsed.value();

    auto reader = get_reader(app_config.input_file);
    if (!reader) {
        return reader.error();
    }
    auto translator = get_translator(app_config.backend, app_config.config);
    if (!translator) {
        return translator.error();
    }
    std::string ext = std::filesystem::path{app_config.output_file}.extension();
    std::optional<StreamWriter> txt_sw;
    std::optional<PdfWriter> pdf_pw;
    FormattedWriter formatted_write;

    Translator postprocessor;
    if (app_config.postprocess == "morpheus") {
        postprocessor = make_morpheus_macron_translator(app_config.breves);
    } else if (app_config.postprocess == "chat-api") {
        try {
            postprocessor =
                make_chat_api_macron_translator(selected_chat_api_config(app_config.config));
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return exit_code::usage_error;
        }
    } else if (app_config.postprocess == "none") {
        postprocessor = pass_translator;
    } else {
        spdlog::error("unknown postprocessor: {}", app_config.postprocess);
        return exit_code::usage_error;
    }

    if (ext == ".txt") {
        txt_sw.emplace(txt_writer(app_config.output_file));
        if (!txt_sw->is_open()) {
            spdlog::error("cannot open output file: {}", app_config.output_file);
            return exit_code::output_error;
        }
        formatted_write = make_formatted_writer(*txt_sw, plain_formatter);
    } else if (ext == ".pdf") {
        pdf_pw.emplace(app_config.output_file);
        if (!pdf_pw->is_open()) {
            spdlog::error("cannot open output file: {}", app_config.output_file);
            return exit_code::output_error;
        }
        formatted_write = make_pdf_formatted_writer(*pdf_pw);
    } else {
        spdlog::error("unsupported output extension: {}", ext);
        return exit_code::usage_error;
    }

    return translate_file(*reader, *translator, postprocessor, formatted_write, app_config.input_file,
                          app_config.parallelism);
}
