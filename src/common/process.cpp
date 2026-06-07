#include "process.hpp"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

void exec_child(const std::string& binary_path,
                const std::vector<std::pair<std::string, std::string>>& env_vars,
                const std::vector<std::string>& args) {
    for (const auto& [key, value] : env_vars) {
        setenv(key.c_str(), value.c_str(), 1);
    }

    // This is where the child process starts specified binary execution.
    // execvp searches PATH for binaries, so absolute path is not required.
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(binary_path.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(binary_path.c_str(), argv.data());
}

} // namespace

// This logic assumes that input never exceeds
// unix buffer size, 64kb by default
// The process to be run should accept text input via stdin
// and provide text output from stdout
ProcessResult run_process(const std::string& binary_path, const std::string& input,
                          const std::vector<std::pair<std::string, std::string>>& env_vars,
                          const std::vector<std::string>& args, bool suppress_output) {
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) == -1) {
        throw std::runtime_error("pipe() failed");
    }
    if (pipe(pipe_out) == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        throw std::runtime_error("pipe() failed");
    }

    pid_t pid = fork();
    // At this point we have two processes, parent with non-0 pid
    // -1 means the fork failed
    // 0 is what child gets and it allows us to detect
    // that we're in a child process
    // pid > 0 means we're running inside parent process
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        throw std::runtime_error("fork() failed");
    }

    // This part is executed inside child process
    if (pid == 0) {
        // [0] is read, [1] is write
        // For consuming process input pipe doesn't write, close its descriptor
        close(pipe_in[1]);
        // Output pipe doesn't read
        close(pipe_out[0]);

        // Make file descriptor 0 (stdin) point to the read end of the input pipe.
        // From now on, anything the child reads from stdin comes from the pipe.
        dup2(pipe_in[0], STDIN_FILENO);
        // Make file descriptor 1 (stdout) point to the write end of the output pipe.
        // Anything the child writes to stdout goes into the pipe.
        dup2(pipe_out[1], STDOUT_FILENO);
        if (suppress_output) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null != -1) {
                dup2(dev_null, STDERR_FILENO);
                close(dev_null);
            }
        }

        // Close rest of the original in/out pipes after duplication
        close(pipe_in[0]);
        close(pipe_out[1]);

        exec_child(binary_path, env_vars, args);
        // This code will never be reached if execl run successfully
        // as it substitutes the process image
        _exit(127);
    }

    // Parent proceeds here, skipping child logic
    close(pipe_in[0]);
    close(pipe_out[1]);

    const char* data = input.data();
    size_t remaining = input.size();
    while (remaining > 0) {
        ssize_t written = write(pipe_in[1], data, remaining);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            break;
        }
        data += written;
        remaining -= written;
    }
    close(pipe_in[1]);

    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipe_out[0], buf, sizeof(buf))) > 0) {
        output.append(buf, n);
    }
    close(pipe_out[0]);

    int status;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return {std::move(output), exit_code};
}

PersistentProcess::PersistentProcess(
    const std::string& binary_path,
    const std::vector<std::pair<std::string, std::string>>& env_vars,
    const std::vector<std::string>& args, bool suppress_output) {
    int pipe_in[2];
    int pipe_out[2];

    // pipe_in carries parent writes to child stdin:
    //   pipe_in[1] is kept by the parent, pipe_in[0] becomes the child's stdin.
    // pipe_out carries child stdout back to parent reads:
    //   pipe_out[1] becomes the child's stdout, pipe_out[0] is kept by parent.
    if (pipe(pipe_in) == -1) {
        throw std::runtime_error("pipe() failed");
    }
    if (pipe(pipe_out) == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        throw std::runtime_error("pipe() failed");
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        throw std::runtime_error("fork() failed");
    }

    if (child_pid == 0) {
        // Child only reads from pipe_in and writes to pipe_out. Closing the
        // unused ends is important: if an extra write end stays open, the
        // reader may never see EOF when the real writer exits.
        close(pipe_in[1]);
        close(pipe_out[0]);

        // dup2 maps the pipe ends onto the standard file descriptors expected
        // by normal Unix programs. After this, the child can simply read stdin
        // and write stdout; it does not know the parent is using pipes.
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        if (suppress_output) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null != -1) {
                dup2(dev_null, STDERR_FILENO);
                close(dev_null);
            }
        }

        close(pipe_in[0]);
        close(pipe_out[1]);

        exec_child(binary_path, env_vars, args);
        _exit(127);
    }

    // Parent keeps the opposite ends: write requests to child stdin, read
    // responses from child stdout.
    close(pipe_in[0]);
    close(pipe_out[1]);
    stdin_fd_ = pipe_in[1];
    stdout_fd_ = pipe_out[0];
    pid_ = child_pid;
}

PersistentProcess::~PersistentProcess() {
    close_and_wait();
}

PersistentProcess::PersistentProcess(PersistentProcess&& other) noexcept
    : stdin_fd_(std::exchange(other.stdin_fd_, -1)),
      stdout_fd_(std::exchange(other.stdout_fd_, -1)), pid_(std::exchange(other.pid_, -1)),
      read_buffer_(std::move(other.read_buffer_)),
      exit_code_(std::exchange(other.exit_code_, std::nullopt)) {}

PersistentProcess& PersistentProcess::operator=(PersistentProcess&& other) noexcept {
    if (this != &other) {
        close_and_wait();
        stdin_fd_ = std::exchange(other.stdin_fd_, -1);
        stdout_fd_ = std::exchange(other.stdout_fd_, -1);
        pid_ = std::exchange(other.pid_, -1);
        read_buffer_ = std::move(other.read_buffer_);
        exit_code_ = std::exchange(other.exit_code_, std::nullopt);
    }
    return *this;
}

void PersistentProcess::write_all(std::string_view input) {
    const char* data = input.data();
    size_t remaining = input.size();
    while (remaining > 0) {
        // write() may accept only part of the buffer, especially for pipes.
        // Keep advancing until the full request has been handed to the kernel.
        ssize_t written = write(stdin_fd_, data, remaining);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("write() failed");
        }
        data += written;
        remaining -= written;
    }
}

std::optional<std::string> PersistentProcess::read_line() {
    while (true) {
        // Return a complete line if one is already buffered.
        auto newline_pos = read_buffer_.find('\n');
        if (newline_pos != std::string::npos) {
            // A single read() can return multiple stdout lines or a partial
            // line. Return one complete line and leave the rest buffered.
            auto line = read_buffer_.substr(0, newline_pos);
            read_buffer_.erase(0, newline_pos + 1);
            // Remove trailing \r in case like ends with \r\n
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        }

        // No full line is buffered yet, so read more child stdout.
        char buf[4096];
        ssize_t n = read(stdout_fd_, buf, sizeof(buf));
        if (n > 0) {
            read_buffer_.append(buf, n);
            continue;
        }
        if (n == -1 && errno == EINTR)
            continue;

        // Stdout ended, but buffered bytes still form a final line.
        if (!read_buffer_.empty()) {
            auto line = std::move(read_buffer_);
            read_buffer_.clear();
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        }

        // Stdout ended and nothing remains buffered.
        return std::nullopt;
    }
}

int PersistentProcess::close_and_wait() {
    // Closing stdin tells well-behaved filter-style children that no more input
    // is coming. For Morpheus this is what lets its stdin loop finish normally.
    if (stdin_fd_ != -1) {
        close(stdin_fd_);
        stdin_fd_ = -1;
    }
    // This object no longer needs to read from the child. Closing stdout_fd_
    // also prevents the descriptor from leaking if the child exits badly.
    if (stdout_fd_ != -1) {
        close(stdout_fd_);
        stdout_fd_ = -1;
    }

    // No waitable child remains. This happens after close_and_wait() already
    // ran, after running() reaped an exited child with WNOHANG, or on a
    // moved-from object. Preserve a cached exit code when one exists; otherwise
    // keep the old "nothing to close" result of 0.
    if (pid_ <= 0)
        return exit_code_.value_or(0);

    int status = 0;
    while (waitpid(pid_, &status, 0) == -1) {
        // Signals can interrupt waitpid before it observes the child status.
        // Retry because the child is still our responsibility to reap.
        if (errno == EINTR)
            continue;

        // Any other waitpid failure means we cannot obtain a reliable child
        // status. Mark the process as no longer waitable and cache failure.
        pid_ = -1;
        exit_code_ = -1;
        return exit_code_.value();
    }

    // waitpid returned this child, so status is valid and the child has been
    // reaped. Cache the normal exit code; use -1 for signal termination or
    // other non-normal exits where there is no process exit code.
    pid_ = -1;
    exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return exit_code_.value();
}

bool PersistentProcess::running() {
    // No child pid is currently owned by this object. The child may never have
    // started, may have been moved away, or may already have been reaped.
    if (pid_ <= 0)
        return false;

    int status = 0;
    pid_t result = waitpid(pid_, &status, WNOHANG);

    if (result == 0) {
        // WNOHANG makes waitpid poll instead of block. A zero result means the
        // child is still running and no status was available to reap.
        return true;
    }

    if (result == -1 && errno == EINTR) {
        // A signal interrupted the status check before waitpid could answer.
        // Treat the process as running so a later call can retry the poll.
        return true;
    }

    if (result == pid_) {
        // The child exited before this check. waitpid has reaped it and filled
        // status, so cache the decoded result for close_and_wait().
        pid_ = -1;
        exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return false;
    }

    // ECHILD or another waitpid failure: there is no live waitable child whose
    // exit status we can collect, and status is not meaningful in this branch.
    pid_ = -1;
    exit_code_ = -1;
    return false;
}
