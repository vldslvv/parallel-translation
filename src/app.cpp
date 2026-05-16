#include "app.hpp"

#include <chrono>

#include <deque>
#include <expected>
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

static std::expected<Reader, int> get_reader(const ReaderConfig& config) {
    if (config.format == "txt") {
        return txt_reader;
    }
    if (config.format == "pdf") {
        return pdf_reader;
    }

    spdlog::error("unsupported reader format: {}", config.format);
    return std::unexpected(exit_code::usage_error);
}

static std::expected<Translator, int> get_translator(const BackendConfig& config) {
    if (config.provider == "stub") {
        return stub_translator;
    }
    if (config.provider == "pass") {
        return pass_translator;
    }
    if (config.provider == "chat-api") {
        try {
            return make_chat_api_latin_to_english_translator(config.chat_api);
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return std::unexpected(exit_code::usage_error);
        }
    }

    spdlog::error("unknown backend provider: {}", config.provider);
    return std::unexpected(exit_code::usage_error);
}

static std::expected<Translator, int> get_postprocessor(const PostprocessingConfig& config) {
    if (config.provider == "morpheus") {
        return make_morpheus_macron_translator(config.breves);
    }
    if (config.provider == "chat-api") {
        try {
            return make_chat_api_macron_translator(config.chat_api);
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return std::unexpected(exit_code::usage_error);
        }
    }
    if (config.provider == "none") {
        return pass_translator;
    }

    spdlog::error("unknown postprocessor provider: {}", config.provider);
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

    auto reader = get_reader(config.reader);
    if (!reader) {
        return reader.error();
    }
    auto translator = get_translator(config.backend);
    if (!translator) {
        return translator.error();
    }
    auto postprocessor = get_postprocessor(config.postprocessing);
    if (!postprocessor) {
        return postprocessor.error();
    }

    std::optional<StreamWriter> txt_sw;
    std::optional<PdfWriter> pdf_pw;
    FormattedWriter formatted_write;

    if (config.writer.format == "txt") {
        txt_sw.emplace(txt_writer(config.writer.path));
        if (!txt_sw->is_open()) {
            spdlog::error("cannot open output file: {}", config.writer.path);
            return exit_code::output_error;
        }
        formatted_write = make_formatted_writer(*txt_sw, plain_formatter);
    } else if (config.writer.format == "pdf") {
        pdf_pw.emplace(config.writer.path);
        if (!pdf_pw->is_open()) {
            spdlog::error("cannot open output file: {}", config.writer.path);
            return exit_code::output_error;
        }
        formatted_write = make_pdf_formatted_writer(*pdf_pw);
    } else {
        spdlog::error("unsupported writer format: {}", config.writer.format);
        return exit_code::usage_error;
    }

    return translate_file(*reader, *translator, *postprocessor, formatted_write, config.reader.path,
                          config.backend.parallelism);
}
