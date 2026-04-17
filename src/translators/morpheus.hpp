#pragma once
#include "translator.hpp"
#include <string>

// morpheus_base: path prefix for ccode/morpheus (e.g. $HOME); defaults to $HOME if empty
// render_breves: when true, short vowels are marked with a breve (ă ĕ ĭ ŏ ŭ)
Translator make_morpheus_macron_translator(const std::string& morpheus_base = "",
                                           bool render_breves = false);
