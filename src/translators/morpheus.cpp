#include "morpheus.hpp"
#include "common/process.hpp"
#include "text/text.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <unistd.h>

namespace {

struct MorpheusRuntimePaths {
    std::string cruncher;
    std::string helper_dir;
    std::string stemlib;
};

MorpheusRuntimePaths morpheus_runtime_paths() {
    std::error_code ec;
    std::array<char, 4096> buf{};
    // Linux exposes the current executable path through this procfs symlink.
    ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);

    if (n > 0) {
        // Installed runs should use the private Morpheus copy installed next
        // to parallel-translation, not the Conan cache. Derive the install
        // prefix by removing CMAKE_INSTALL_BINDIR from the executable path.
        auto exe = std::filesystem::path{std::string{buf.data(), static_cast<size_t>(n)}};
        auto bindir = std::filesystem::path{PT_INSTALL_BINDIR};
        auto depth = std::distance(bindir.begin(), bindir.end());
        auto prefix = exe.parent_path();
        for (decltype(depth) i = 0; i < depth && !prefix.empty(); ++i)
            prefix = prefix.parent_path();

        MorpheusRuntimePaths installed{
            (prefix / PT_MORPHEUS_INSTALL_LIBEXEC_DIR / "bin" / "cruncher").string(),
            (prefix / PT_MORPHEUS_INSTALL_LIBEXEC_DIR / "libexec" / "morpheus").string(),
            (prefix / PT_MORPHEUS_INSTALL_SHARE_DIR / "stemlib").string(),
        };
        if (std::filesystem::exists(installed.cruncher, ec))
            return installed;
    }

    // Build-tree runs do not have an installed private copy yet, so use the
    // Morpheus package resolved by Conan at configure time.
    auto root = std::filesystem::path{PT_MORPHEUS_PACKAGE_DIR};
    return {
        (root / "bin" / "cruncher").string(),
        (root / "libexec" / "morpheus").string(),
        (root / "res" / "stemlib").string(),
    };
}

// Convert Perseus quantity notation to Unicode.
// '_' after a vowel → macron (ā ē ī ō ū / Ā Ē Ī Ō Ū).
// '^' after a vowel → breve (ă ĕ ĭ ŏ ŭ / Ă Ĕ Ĭ Ŏ Ŭ) when render_breves is true, else dropped.
std::string perseus_to_unicode(std::string_view s, bool render_breves) {
    static constexpr std::string_view vowels = "aeiouAEIOU";
    static constexpr const char* macrons[] = {"ā", "ē", "ī", "ō", "ū", "Ā", "Ē", "Ī", "Ō", "Ū"};
    static constexpr const char* breves[] = {"ă", "ĕ", "ĭ", "ŏ", "ŭ", "Ă", "Ĕ", "Ĭ", "Ŏ", "Ŭ"};

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
std::string extract_first_form(std::string_view line, bool render_breves) {
    auto nl_start = line.find("<NL>");
    if (nl_start == std::string_view::npos)
        return {};
    auto content_start = nl_start + 4; // skip "<NL>"
    auto nl_end = line.find("</NL>", content_start);
    std::string_view block =
        line.substr(content_start, nl_end == std::string_view::npos ? std::string_view::npos
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
std::unordered_map<std::string, std::string> parse_cruncher_output(const std::string& output,
                                                                   bool render_breves) {
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

bool should_suppress_cruncher_output() {
    return spdlog::get_level() > spdlog::level::debug;
}

class MorpheusSession {
  public:
    MorpheusSession(MorpheusRuntimePaths paths, std::string path_env)
        : paths_{std::move(paths)}, path_env_{std::move(path_env)} {}

    std::string analyze(const std::string& input) {
        std::lock_guard lock{mutex_};
        if (disabled_)
            return analyze_one_shot(input);

        try {
            ensure_process();
            // A persistent stdin/stdout pipe has no per-request EOF marker:
            // EOF would terminate the whole cruncher process. Morpheus echoes
            // lines starting with '#', so a unique comment line is a cheap
            // request boundary that comes back on stdout after this batch.
            const auto sentinel = "#PT_END_" + std::to_string(++request_id_);
            process_->write_all(input);
            process_->write_all(sentinel);
            process_->write_all("\n");

            std::string output;
            while (auto line = process_->read_line()) {
                if (*line == sentinel)
                    return output;
                output += *line;
                output += '\n';
            }
            throw std::runtime_error("morpheus process closed stdout");
        } catch (const std::exception& e) {
            spdlog::warn("morpheus persistent process failed, falling back to one-shot: {}",
                         e.what());
            process_.reset();
            disabled_ = true;
            return analyze_one_shot(input);
        }
    }

  private:
    void ensure_process() {
        if (process_ && process_->running())
            return;

        spdlog::debug("morpheus: cruncher={}", paths_.cruncher);
        // cruncher already reads stdin in a loop, but when stdout is a pipe it
        // block-buffers output and the parent would not see each response until
        // much later. stdbuf -oL forces line buffering without patching Morpheus.
        auto env = std::vector<std::pair<std::string, std::string>>{
            {"PATH", path_env_},
            {"MORPHLIB", paths_.stemlib},
        };
        auto args = std::vector<std::string>{"-oL", paths_.cruncher, "-L"};

        process_.emplace("stdbuf", std::move(env), std::move(args),
                         should_suppress_cruncher_output());
    }

    std::string analyze_one_shot(const std::string& input) const {
        auto env = std::vector<std::pair<std::string, std::string>>{
            {"PATH", path_env_},
            {"MORPHLIB", paths_.stemlib},
        };
        auto args = std::vector<std::string>{"-L"};

        auto result = run_process(paths_.cruncher, input, std::move(env), std::move(args),
                                  should_suppress_cruncher_output());
        spdlog::debug("morpheus: exit={} output_len={}", result.exit_code, result.output.size());
        return result.output;
    }

    MorpheusRuntimePaths paths_;
    std::string path_env_;
    std::mutex mutex_;
    std::optional<PersistentProcess> process_;
    uint64_t request_id_ = 0;
    bool disabled_ = false;
};

} // namespace

Translator make_morpheus_macron_translator(bool render_breves) {
    const auto paths = morpheus_runtime_paths();
    const std::string path_env =
        paths.helper_dir + ":" + (std::getenv("PATH") ? std::getenv("PATH") : "");
    // Translator is std::function, so the returned lambda must be copyable;
    // shared_ptr keeps the session alive across lambda copies.
    const auto session = std::make_shared<MorpheusSession>(paths, path_env);

    return [render_breves, session](std::string_view text) -> std::string {
        auto split = split_words(std::string{text});
        if (split.words.empty())
            return std::string{text};

        // Build cruncher input: one word per line
        std::string input;
        input.reserve(text.size() + split.words.size());
        for (const auto& w : split.words)
            input += w + '\n';

        auto output = session->analyze(input);
        spdlog::debug("morpheus: output_len={}", output.size());
        auto word_map = parse_cruncher_output(output, render_breves);

        SplitText out = split;
        for (size_t i = 0; i < split.words.size(); ++i) {
            auto it = word_map.find(split.words[i]);
            if (it != word_map.end())
                out.words[i] = it->second;
            // else: keep original
        }
        auto reconstructed = reconstruct(out);
        spdlog::debug("morpheus macron: input: {}\n  output: {}", text, reconstructed);
        return reconstructed;
    };
}
