#include "morpheus.hpp"
#include "text/text.hpp"
#include "common/process.hpp"

#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

// Convert Perseus quantity notation to Unicode.
// '_' after a vowel → macron (ā ē ī ō ū / Ā Ē Ī Ō Ū).
// '^' after a vowel → breve (ă ĕ ĭ ŏ ŭ / Ă Ĕ Ĭ Ŏ Ŭ) when render_breves is true, else dropped.
static std::string perseus_to_unicode(std::string_view s, bool render_breves) {
    static constexpr std::string_view vowels  = "aeiouAEIOU";
    static constexpr const char*      macrons[] = {"ā","ē","ī","ō","ū","Ā","Ē","Ī","Ō","Ū"};
    static constexpr const char*      breves[]  = {"ă","ĕ","ĭ","ŏ","ŭ","Ă","Ĕ","Ĭ","Ŏ","Ŭ"};

    std::string result;
    result.reserve(s.size() * 2);
    for (char c : s) {
        if ((c == '_' || c == '^') && !result.empty()) {
            char prev = result.back();
            auto pos = vowels.find(prev);
            if (pos != std::string_view::npos) {
                // prev is a plain ASCII vowel: replace it with the marked form.
                result.pop_back();
                if (c == '_') {
                    result += macrons[pos];
                } else if (render_breves) {
                    result += breves[pos];
                } else {
                    result += prev;
                }
            }
            // else: drop the marker. Two cases reach here:
            //
            // 1. Compound-boundary '^': Perseus uses '^' between morpheme
            //    boundaries in compound words (e.g. "su_^pra_" = sūprā, where
            //    '^' separates the prefix "su-" from the stem "-pra"). In this
            //    case prev is a consonant, so pos == npos and '^' is dropped.
            //
            // 2. Consecutive markers on the same vowel: if '_' already
            //    converted the vowel to a two-byte UTF-8 sequence (e.g. u→ū),
            //    result.back() is the trailing continuation byte (0x80–0xBF),
            //    which is not in the ASCII vowel set. A following '^' (which
            //    Perseus does occasionally emit for such forms) is therefore
            //    also dropped rather than leaked into the output.
        } else {
            result += c;
        }
    }
    return result;
}

// Extract the macronized inflected form from the first <NL>…</NL> block on a line.
// Format: <NL>P form,lemma  grammar</NL>…
//   P   = one-letter part-of-speech code
//   form terminates at first ',', '\t', or ' '
//   lemma may carry a '#N' disambiguation suffix (we don't care about it)
static std::string extract_first_form(std::string_view line, bool render_breves) {
    auto nl_start = line.find("<NL>");
    if (nl_start == std::string_view::npos)
        return {};
    auto content_start = nl_start + 4; // skip "<NL>"
    auto nl_end = line.find("</NL>", content_start);
    std::string_view block = line.substr(content_start,
                                         nl_end == std::string_view::npos
                                             ? std::string_view::npos
                                             : nl_end - content_start);

    // Skip part-of-speech letter(s) + following space(s)
    size_t i = 0;
    while (i < block.size() && (std::isupper(static_cast<unsigned char>(block[i])) != 0))
        ++i;
    while (i < block.size() && block[i] == ' ')
        ++i;

    // Read form up to first delimiter
    size_t form_start = i;
    while (i < block.size() && block[i] != ',' && block[i] != '\t' && block[i] != ' ')
        ++i;
    std::string_view raw_form = block.substr(form_start, i - form_start);

    // Strip trailing '#N' disambiguation suffix
    auto hash = raw_form.rfind('#');
    if (hash != std::string_view::npos)
        raw_form = raw_form.substr(0, hash);

    return perseus_to_unicode(raw_form, render_breves);
}

// Parse the full cruncher output into a word→macronized map.
// The output contains word echo lines interspersed with <NL>…</NL> analysis lines.
// Lines beginning with ':' are timing/cache messages and are skipped.
static std::unordered_map<std::string, std::string>
parse_cruncher_output(const std::string& output, bool render_breves) {
    std::unordered_map<std::string, std::string> word_map;
    std::string current_word;

    size_t pos = 0;
    while (pos <= output.size()) {
        // Find next line
        size_t end = output.find('\n', pos);
        std::string_view line = (end == std::string::npos)
                                    ? std::string_view{output}.substr(pos)
                                    : std::string_view{output}.substr(pos, end - pos);
        pos = (end == std::string::npos) ? output.size() + 1 : end + 1;

        // Strip trailing '\r'
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        if (line.empty())
            continue;
        if (line.front() == ':')
            continue; // timing / cache message

        if (line.starts_with("<NL>")) {
            if (!current_word.empty()) {
                auto form = extract_first_form(line, render_breves);
                if (!form.empty())
                    word_map.emplace(current_word, std::move(form));
                current_word.clear();
            }
        } else {
            // This is a word echo. If we have a pending word with no analysis yet,
            // it was unrecognized — leave it out of the map (caller keeps original).
            current_word = std::string{line};
        }
    }
    return word_map;
}

Translator make_morpheus_macron_translator(const std::string& morpheus_base,
                                           bool render_breves) {
    return [morpheus_base, render_breves](std::string_view text) -> std::string {
        auto split = split_words(std::string{text});
        if (split.words.empty())
            return std::string{text};

        // Build cruncher input: one word per line
        std::string input;
        input.reserve(text.size() + split.words.size());
        for (const auto& w : split.words)
            input += w + '\n';

        std::string base;
        if (!morpheus_base.empty()) {
            base = morpheus_base;
        } else {
            const char* home = std::getenv("HOME");
            base = home ? home : "";
        }

        std::string cruncher = base + "/ccode/morpheus/bin/cruncher";
        std::string path_env = base + "/ccode/morpheus/src/gkends:" +
                               base + "/ccode/morpheus/src/gkdict:" +
                               base + "/ccode/morpheus/src/gener:"  +
                               base + "/ccode/morpheus/src/anal:"   +
                               (std::getenv("PATH") ? std::getenv("PATH") : "");
        std::string morphlib = base + "/ccode/morpheus/stemlib";

        auto result = run_process(cruncher, input,
                                  {{"PATH", path_env}, {"MORPHLIB", morphlib}},
                                  {"-L"});

        spdlog::debug("morpheus: exit={} output_len={}", result.exit_code, result.output.size());

        auto word_map = parse_cruncher_output(result.output, render_breves);

        SplitText out = split;
        for (size_t i = 0; i < split.words.size(); ++i) {
            auto it = word_map.find(split.words[i]);
            if (it != word_map.end())
                out.words[i] = it->second;
            // else: keep original
        }
        return reconstruct(out);
    };
}
