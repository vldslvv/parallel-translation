#pragma once

namespace exit_code {
constexpr int success = 0;
constexpr int runtime_error = 1;
constexpr int usage_error = 2;
constexpr int input_error = 3;
constexpr int output_error = 4;
constexpr int backend_unavailable = 5;
} // namespace exit_code
