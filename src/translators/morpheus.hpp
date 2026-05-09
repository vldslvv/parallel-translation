#pragma once
#include "translator.hpp"
#include <string>

// morpheus_dir: Morpheus root directory
// render_breves: when true, short vowels are marked with a breve (ă ĕ ĭ ŏ ŭ)
Translator make_morpheus_macron_translator(const std::string& morpheus_dir,
                                           bool render_breves = false);
