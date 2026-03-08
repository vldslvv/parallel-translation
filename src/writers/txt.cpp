#include "writer.hpp"
#include <spdlog/spdlog.h>

StreamWriter::StreamWriter(std::string_view path) : file_{std::string{path}}, path_{path} {
    if (file_)
        spdlog::debug("opened {} for writing", path_);
    else
        spdlog::error("cannot open file: {}", path_);
}

bool StreamWriter::is_open() const { return file_.is_open(); }

std::expected<void, std::string> StreamWriter::write(std::string_view chunk) {
    file_ << chunk;
    if (!file_)
        return std::unexpected{"failed to write to: " + path_};
    return {};
}

StreamWriter txt_writer(std::string_view path) { return StreamWriter{path}; }
