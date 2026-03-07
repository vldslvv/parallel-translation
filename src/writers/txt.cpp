#include "writer.hpp"
#include <fstream>

std::expected<void, std::string> txt_writer(std::string_view path, std::string_view content) {
    std::ofstream file{std::string{path}};
    if (!file)
        return std::unexpected{"cannot open file: " + std::string{path}};

    file << content;
    if (!file)
        return std::unexpected{"failed to write to file: " + std::string{path}};

    return {};
}
