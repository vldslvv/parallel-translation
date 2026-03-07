#include "reader.hpp"
#include <fstream>
#include <sstream>

std::expected<std::string, std::string> txt_reader(std::string_view path) {
    std::ifstream file{std::string{path}};
    if (!file)
        return std::unexpected{"cannot open file: " + std::string{path}};

    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}
