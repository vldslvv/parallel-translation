#pragma once
#include <expected>
#include <functional>
#include <generator>
#include <string>
#include <string_view>

using Reader = std::function<
    std::generator<std::expected<std::string, std::string>>(std::string_view)
>;

std::generator<std::expected<std::string, std::string>> txt_reader(std::string_view path);
