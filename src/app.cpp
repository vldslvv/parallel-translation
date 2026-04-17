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

static std::expected<Translator, int> get_translator(const std::string& backend,
                                                     const Config& cfg) {
    if (backend == "stub") {
        return stub_translator;
    }
    if (backend == "pass") {
        return pass_translator;
    }
    if (backend == "ollama") {
        return make_ollama_latin_to_english_translator(cfg.ollama_model, cfg.ollama_host);
    }

    spdlog::error("unknown backend: {}", backend);
    return std::unexpected(exit_code::usage_error);
}

static std::expected<Reader, int> get_reader(std::string& input_file) {
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
    Config cfg = load_config();

    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", "0.1.0");

    std::string input;
    std::string output;
    std::string backend = "ollama";
    std::string postprocess = "morpheus";
    bool breves = false;
    std::string model;
    std::string host;
    std::string log_level;
    int parallelism = cfg.parallelism;

    app.add_option("--input,-i", input, "Input file path")->required();
    app.add_option("--output,-o", output, "Output file path")->required();
    app.add_option("--backend", backend, "Translation backend: ollama, stub, pass")
        ->capture_default_str();
    app.add_option("--postprocess", postprocess,
                   "Macron postprocessor: morpheus, ollama, none")
        ->capture_default_str();
    app.add_flag("--breves", breves, "Mark short vowels with a breve (morpheus only)");
    app.add_option("--ollama-model", model, "Ollama model name (overrides config)");
    app.add_option("--ollama-host", host, "Ollama host URL (overrides config)");
    app.add_option("--log-level", log_level, "Log level: trace/debug/info/warn/error/critical/off");
    app.add_option("--parallelism", parallelism, "Max concurrent translations")
        ->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    if (!model.empty())
        cfg.ollama_model = model;
    if (!host.empty())
        cfg.ollama_host = host;
    if (!log_level.empty())
        cfg.log_level = log_level;
    spdlog::set_level(spdlog::level::from_str(cfg.log_level));
    spdlog::debug("config: file={}", cfg.config_file.empty() ? "(none)" : cfg.config_file);
    spdlog::debug("config: ollama_host={}", cfg.ollama_host);
    spdlog::debug("config: ollama_model={}", cfg.ollama_model);
    spdlog::debug("config: source_lang={}", cfg.source_lang);
    spdlog::debug("config: target_lang={}", cfg.target_lang);
    spdlog::debug("config: log_level={}", cfg.log_level);
    spdlog::debug("config: parallelism={}", parallelism);

    auto reader = get_reader(input);
    if (!reader) {
        return reader.error();
    }
    auto translator = get_translator(backend, cfg);
    if (!translator) {
        return translator.error();
    }
    std::string ext = std::filesystem::path{output}.extension();
    std::optional<StreamWriter> txt_sw;
    std::optional<PdfWriter> pdf_pw;
    FormattedWriter formatted_write;

    Translator postprocessor;
    if (postprocess == "morpheus") {
        postprocessor = make_morpheus_macron_translator("", breves);
    } else if (postprocess == "ollama") {
        postprocessor = make_ollama_macron_translator(cfg.ollama_model, cfg.ollama_host);
    } else {
        postprocessor = pass_translator;
    }

    if (ext == ".txt") {
        txt_sw.emplace(txt_writer(output));
        if (!txt_sw->is_open()) {
            spdlog::error("cannot open output file: {}", output);
            return exit_code::output_error;
        }
        formatted_write = make_formatted_writer(*txt_sw, plain_formatter);
    } else if (ext == ".pdf") {
        pdf_pw.emplace(output);
        if (!pdf_pw->is_open()) {
            spdlog::error("cannot open output file: {}", output);
            return exit_code::output_error;
        }
        formatted_write = make_pdf_formatted_writer(*pdf_pw);
    } else {
        spdlog::error("unsupported output extension: {}", ext);
        return exit_code::usage_error;
    }

    return translate_file(*reader, *translator, postprocessor, formatted_write, input, parallelism);
}
