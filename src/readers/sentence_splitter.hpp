#pragma once
#include <generator>
#include <string>

std::generator<std::string> split_sentences(std::generator<std::string> chunks);
