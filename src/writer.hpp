#pragma once
#include <expected>
#include <functional>
#include <string_view>

using Writer = std::function<std::expected<void, std::string>(std::string_view path,
                                                              std::string_view content)>;

std::expected<void, std::string> txt_writer(std::string_view path, std::string_view content);
