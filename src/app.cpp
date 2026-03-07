#include "app.hpp"

#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

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

    app.add_option("--input,-i",   input,   "Input file path")->required();
    app.add_option("--output,-o",  output,  "Output file path")->required();
    app.add_option("--backend",    backend, "Translation backend: ollama, stub")->capture_default_str();
    app.add_option("--ollama-model", model, "Ollama model name (overrides config)");
    app.add_option("--ollama-host",  host,  "Ollama host URL (overrides config)");

    CLI11_PARSE(app, argc, argv);

    if (!model.empty()) cfg.ollama_model = model;
    if (!host.empty())  cfg.ollama_host  = host;

    Reader read         = txt_reader;
    Translator translate = (backend == "stub")
        ? Translator{stub_translator}
        : make_ollama_translator(cfg.ollama_model, cfg.ollama_host);
    Writer write        = txt_writer;

    auto content = read(input);
    if (!content) {
        std::cerr << "error: " << content.error() << "\n";
        return 1;
    }

    auto result = write(output, translate(*content));
    if (!result) {
        std::cerr << "error: " << result.error() << "\n";
        return 1;
    }

    return 0;
}
