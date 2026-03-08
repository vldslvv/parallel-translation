#include "writers/formatted_writer.hpp"

#include <spdlog/spdlog.h>

#include "common/exit_codes.hpp"

FormattedWriter make_formatted_writer(StreamWriter& writer, Formatter format) {
    return [&writer, format](std::string_view original, std::string_view translation) -> int {
        auto out = format(original, translation);
        if (auto r = writer.write(out); !r) {
            spdlog::error("{}", r.error());
            return exit_code::output_error;
        }
        return exit_code::success;
    };
}
