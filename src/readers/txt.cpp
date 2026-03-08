#include "reader.hpp"
#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

std::expected<std::string, std::string> txt_reader(std::string_view path) {
    spdlog::debug("reading file: {}", path);
    std::ifstream file{std::string{path}};
    if (!file)
        return std::unexpected{"cannot open file: " + std::string{path}};

    std::ostringstream buf;
    buf << file.rdbuf();
    auto content = buf.str();
    spdlog::debug("read {} bytes from {}", content.size(), path);
    return content;
}
