#pragma once

#include <fstream>
#include <string>

inline std::string read_file(const char* path) {
    std::ifstream f{path};
    return {std::istreambuf_iterator<char>{f}, {}};
}
