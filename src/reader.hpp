#pragma once
#include <expected>
#include <functional>
#include <string>
#include <string_view>

using Reader = std::function<std::expected<std::string, std::string>(std::string_view path)>;

std::expected<std::string, std::string> txt_reader(std::string_view path);
