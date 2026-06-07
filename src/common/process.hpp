#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ProcessResult {
    std::string output;
    int exit_code;
};

// This logic assumes that input never exceeds
// unix buffer size, 64kb by default
// The process to be run should accept text input via stdin
// and provide text output from stdou
ProcessResult run_process(const std::string& binary_path, const std::string& input,
                          const std::vector<std::pair<std::string, std::string>>& env_vars = {},
                          const std::vector<std::string>& args = {},
                          bool suppress_output = false);

class PersistentProcess {
  public:
    // Starts a child process whose stdin/stdout stay connected to this object.
    // Use this for tools that can handle multiple requests on one stdin stream.
    PersistentProcess(const std::string& binary_path,
                      const std::vector<std::pair<std::string, std::string>>& env_vars = {},
                      const std::vector<std::string>& args = {}, bool suppress_output = false);
    ~PersistentProcess();

    // Disallow copy
    PersistentProcess(const PersistentProcess&) = delete;
    PersistentProcess& operator=(const PersistentProcess&) = delete;
    // Move is not mandatory to have here, but it is implemented
    PersistentProcess(PersistentProcess&& other) noexcept;
    PersistentProcess& operator=(PersistentProcess&& other) noexcept;

    void write_all(std::string_view input);
    // Reads one stdout line without the trailing newline. Extra bytes from the
    // same read() call are kept in read_buffer_ for the next read_line().
    std::optional<std::string> read_line();
    int close_and_wait();
    bool running();

  private:
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    int pid_ = -1;
    std::string read_buffer_;
    std::optional<int> exit_code_;
};
