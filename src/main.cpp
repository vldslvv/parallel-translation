#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

static constexpr auto VERSION = "0.1.0";

int main(int argc, char* argv[]) {
    CLI::App app{"parallel-translation"};
    app.set_version_flag("--version,-v", VERSION);

    std::string input;
    std::string output;

    app.add_option("--input,-i", input, "Input file path")->required();
    app.add_option("--output,-o", output, "Output file path")->required();

    CLI11_PARSE(app, argc, argv);

    std::cout << "Input:  " << input << "\n";
    std::cout << "Output: " << output << "\n";

    return 0;
}
