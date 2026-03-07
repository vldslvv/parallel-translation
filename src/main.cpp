#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

#include "reader.hpp"
#include "translator.hpp"
#include "writer.hpp"

static constexpr auto VERSION = "0.1.0";

int main(int argc, char* argv[]) {
    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", VERSION);

    std::string input;
    std::string output;

    app.add_option("--input,-i", input, "Input file path")->required();
    app.add_option("--output,-o", output, "Output file path")->required();

    CLI11_PARSE(app, argc, argv);

    Reader read     = txt_reader;
    Translator translate = stub_translator;
    Writer write    = txt_writer;

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
