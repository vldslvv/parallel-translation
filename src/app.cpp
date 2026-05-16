#include "app.hpp"

#include <chrono>

#include <deque>
#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <semaphore>
#include <string>

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
            return make_chat_api_latin_to_english_translator(cfg.chat_api);
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
    Config& config = parsed.value();

    auto reader = get_reader(config.input_file);
    if (!reader) {
        return reader.error();
    }
    auto translator = get_translator(config.backend, config);
    if (!translator) {
        return translator.error();
    }
    std::string ext = std::filesystem::path{config.output_file}.extension();
    std::optional<StreamWriter> txt_sw;
    std::optional<PdfWriter> pdf_pw;
    FormattedWriter formatted_write;

    Translator postprocessor;
    if (config.postprocess == "morpheus") {
        postprocessor = make_morpheus_macron_translator(config.breves);
    } else if (config.postprocess == "chat-api") {
        try {
            postprocessor = make_chat_api_macron_translator(config.chat_api);
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return exit_code::usage_error;
        }
    } else if (config.postprocess == "none") {
        postprocessor = pass_translator;
    } else {
        spdlog::error("unknown postprocessor: {}", config.postprocess);
        return exit_code::usage_error;
    }

    if (ext == ".txt") {
        txt_sw.emplace(txt_writer(config.output_file));
        if (!txt_sw->is_open()) {
            spdlog::error("cannot open output file: {}", config.output_file);
            return exit_code::output_error;
        }
        formatted_write = make_formatted_writer(*txt_sw, plain_formatter);
    } else if (ext == ".pdf") {
        pdf_pw.emplace(config.output_file);
        if (!pdf_pw->is_open()) {
            spdlog::error("cannot open output file: {}", config.output_file);
            return exit_code::output_error;
        }
        formatted_write = make_pdf_formatted_writer(*pdf_pw);
    } else {
        spdlog::error("unsupported output extension: {}", ext);
        return exit_code::usage_error;
    }

    return translate_file(*reader, *translator, postprocessor, formatted_write, config.input_file,
                          config.parallelism);
}
