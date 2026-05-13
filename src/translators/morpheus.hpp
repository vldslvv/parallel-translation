#pragma once
#include "translator.hpp"

// render_breves: when true, short vowels are marked with a breve (ă ĕ ĭ ŏ ŭ)
Translator make_morpheus_macron_translator(bool render_breves = false);
