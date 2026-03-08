#include "app.hpp"

#include <chrono>
#include <deque>
#include <future>
#include <semaphore>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"
#include "common/scope_exit.hpp"
#include "config.hpp"
#include "formatters/formatter.hpp"
#include "readers/reader.hpp"
#include "translators/ollama.hpp"
#include "translators/translator.hpp"
#include "writers/writer.hpp"

static int write_pair(StreamWriter& writer, const Formatter& format, std::string_view original,
                      std::string_view translation) {
    auto out = format(original, translation);
    if (auto r = writer.write(out); !r) {
        spdlog::error("{}", r.error());
        return exit_code::output_error;
    }
    return 0;
}

static int translate_file(const Reader& read, const Translator& translate, const Formatter& format,
                          const Writer& write, std::string_view input, std::string_view output,
                          int parallelism) {
    auto writer = write(output);
    if (!writer.is_open()) {
        spdlog::error("cannot open output file: {}", output);
        return exit_code::output_error;
    }

    constexpr int sem_max = 64;
    if (parallelism > sem_max) {
        spdlog::error("parallelism {} exceeds maximum {}", parallelism, sem_max);
        return exit_code::usage_error;
    }
    std::counting_semaphore<sem_max> sem{parallelism};
    std::deque<std::pair<std::string, std::future<std::string>>> work;

    auto flush = [&work, &writer, &format]() -> int {
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
            if (auto rc = write_pair(writer, format, original, translation); rc != 0)
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
        if (auto rc = write_pair(writer, format, original, translation); rc != 0)
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
    std::string model;
    std::string host;
    std::string log_level;
    int parallelism = cfg.parallelism;

    app.add_option("--input,-i", input, "Input file path")->required();
    app.add_option("--output,-o", output, "Output file path")->required();
    app.add_option("--backend", backend, "Translation backend: ollama, stub, pass")
        ->capture_default_str();
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

    Reader read = txt_reader;
    Formatter format = plain_formatter;
    Translator translate;
    if (backend == "stub")
        translate = stub_translator;
    else if (backend == "pass")
        translate = pass_translator;
    else if (backend == "ollama")
        translate = make_ollama_translator(cfg.ollama_model, cfg.ollama_host);
    else {
        spdlog::error("unknown backend: {}", backend);
        return exit_code::usage_error;
    }
    Writer write = txt_writer;

    return translate_file(read, translate, format, write, input, output, parallelism);
}
