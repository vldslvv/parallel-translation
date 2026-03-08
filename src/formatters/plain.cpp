#include "formatter.hpp"
#include <string>
#include <string_view>

std::string plain_formatter(std::string_view original, std::string_view translation) {
    std::string out;
    out.reserve(original.size() + 1 + translation.size() + 2);
    out += original;
    out += '\n';
    out += translation;
    out += "\n\n";
    return out;
}
