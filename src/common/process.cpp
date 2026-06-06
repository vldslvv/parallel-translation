#include "process.hpp"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

// This logic assumes that input never exceeds
// unix buffer size, 64kb by default
// The process to be run should accept text input via stdin
// and provide text output from stdou
ProcessResult run_process(const std::string& binary_path, const std::string& input,
                          const std::vector<std::pair<std::string, std::string>>& env_vars,
                          const std::vector<std::string>& args, bool suppress_output) {
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
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

        for (const auto& [key, value] : env_vars) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // This is where the child process start specified binary execution
        // execlp searches PATH for binaries, so absolute path is not required
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary_path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(binary_path.c_str(), argv.data());
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
