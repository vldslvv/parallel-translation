#include <iostream>
#include <string_view>

static constexpr std::string_view VERSION = "0.1.0";
static constexpr std::string_view PROGRAM = "parallel-translation";

static void print_version() {
    std::cout << PROGRAM << " " << VERSION << "\n";
}

static void print_help() {
    std::cout << "Usage: " << PROGRAM << " [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  --help      Show this help message and exit\n"
              << "  --version   Show version information and exit\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    const std::string_view arg{argv[1]};

    if (arg == "--help" || arg == "-h") {
        print_help();
    } else if (arg == "--version" || arg == "-v") {
        print_version();
    } else {
        std::cerr << PROGRAM << ": unknown option '" << arg << "'\n"
                  << "Try '" << PROGRAM << " --help' for more information.\n";
        return 1;
    }

    return 0;
}
