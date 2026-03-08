#include "writer.hpp"
#include <fstream>

#include <spdlog/spdlog.h>

std::expected<void, std::string> txt_writer(std::string_view path, std::string_view content) {
    spdlog::debug("writing {} bytes to {}", content.size(), path);
    std::ofstream file{std::string{path}};
    if (!file)
        return std::unexpected{"cannot open file: " + std::string{path}};

    file << content;
    if (!file)
        return std::unexpected{"failed to write to file: " + std::string{path}};

    spdlog::debug("wrote {} successfully", path);
    return {};
}
