#pragma once
#include <expected>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>

class StreamWriter {
  public:
    explicit StreamWriter(std::string_view path);
    StreamWriter(StreamWriter&&) noexcept = default;
    StreamWriter& operator=(StreamWriter&&) noexcept = default;
    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;

    bool is_open() const;
    std::expected<void, std::string> write(std::string_view chunk);
    // ofstream closes/flushes on destruction (RAII)

  private:
    std::ofstream file_;
    std::string path_;
};

// Factory: opens the file, returns a StreamWriter that owns the fd
using Writer = std::function<StreamWriter(std::string_view path)>;

StreamWriter txt_writer(std::string_view path);
