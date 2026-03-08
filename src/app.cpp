#include "app.hpp"

#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "reader.hpp"
#include "translator.hpp"
#include "translators/ollama.hpp"
#include "writer.hpp"

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

    app.add_option("--input,-i", input, "Input file path")->required();
    app.add_option("--output,-o", output, "Output file path")->required();
    app.add_option("--backend", backend, "Translation backend: ollama, stub, pass")
        ->capture_default_str();
    app.add_option("--ollama-model", model, "Ollama model name (overrides config)");
    app.add_option("--ollama-host", host, "Ollama host URL (overrides config)");
    app.add_option("--log-level", log_level, "Log level: trace/debug/info/warn/error/critical/off");

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

    Reader read = txt_reader;
    Translator translate;
    if (backend == "stub")
        translate = stub_translator;
    else if (backend == "pass")
        translate = pass_translator;
    else if (backend == "ollama")
        translate = make_ollama_translator(cfg.ollama_model, cfg.ollama_host);
    else {
        spdlog::error("unknown backend: {}", backend);
        return 1;
    }
    Writer write = txt_writer;

    std::string translated;
    for (const auto& item : read(input)) {
        if (!item) {
            spdlog::error("{}", item.error());
            return 1;
        }
        spdlog::debug("{}", *item);

        // First, write the original sentence
        translated += *item;
        translated += '\n';
        // Then, translated
        translated += translate(*item);
        translated += "\n\n";
    }

    auto result = write(output, translated);
    if (!result) {
        spdlog::error("{}", result.error());
        return 1;
    }

    return 0;
}
