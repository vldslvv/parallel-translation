#pragma once
#include <functional>
#include <string_view>

#include "formatters/formatter.hpp"
#include "writers/writer.hpp"

// A FormattedWriter formats a pair and writes it, returning an exit code
using FormattedWriter = std::function<int(std::string_view original, std::string_view translation)>;

// Factory: captures writer by reference (does NOT take ownership of the fd)
FormattedWriter make_formatted_writer(StreamWriter& writer, Formatter format);
