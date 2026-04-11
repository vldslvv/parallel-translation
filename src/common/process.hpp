#pragma once

#include <string>
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
                          const std::vector<std::string>& args = {});
