#include "ollama.hpp"
#include "text/text.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include <spdlog/spdlog.h>

const std::string translate_latin_to_english_prompt =
    "You translate Latin text to English.\n"
    "Rules:\n"
    "- Return only the English translation, no preamble, commentary, or explanation.\n"
    "- Preserve the paragraph and line structure of the original.\n"
    "- Do not add interpretive notes, alternative readings, or parenthetical remarks.";

const std::string add_macrons_to_latin_prompt =
    "You add macrons to Latin vowels (a\u2192\u0101, e\u2192\u0113, i\u2192\u012b, o\u2192\u014d, "
    "u\u2192\u016b).\n"
    "Rules:\n"
    "- Preserve every word, punctuation mark, and whitespace exactly as given.\n"
    "- Do not add, remove, reorder, or alter any word.\n"
    "- Do not correct typos or grammar.\n"
    "- The output must have the same number of words as the input.\n"
    "- Return only the marked-up text, nothing else.\n\n"
    "Example:\n"
    "Input: Gallia est omnis divisa in partes tres.\n"
    "Output: Gallia est omnis d\u012bv\u012bsa in part\u0113s tr\u0113s.";

static std::string trim_right(std::string result) {
    auto end = result.find_last_not_of(" \t\n\r\f\v");
    if (end != std::string::npos)
        result.erase(end + 1);
    else
        result.clear();
    return result;
}

static std::string chat_api_path(const ChatApiConfig& cfg) {
    if (cfg.provider == "openrouter")
        return "/api/v1/chat/completions";
    if (cfg.provider == "ollama")
        return "/api/chat";
    throw std::runtime_error{"chat-api: unknown provider: " + cfg.provider};
}

static std::string response_content(const ChatApiConfig& cfg, const std::string& body) {
    auto json = nlohmann::json::parse(body);
    if (cfg.provider == "openrouter")
        return json["choices"][0]["message"]["content"].get<std::string>();
    if (cfg.provider == "ollama")
        return json["message"]["content"].get<std::string>();
    throw std::runtime_error{"chat-api: unknown provider: " + cfg.provider};
}

Translator make_chat_api_translator(const ChatApiConfig& cfg, const std::string& prompt) {
    if (cfg.provider == "openrouter" && cfg.api_key.empty())
        throw std::runtime_error{"chat-api: OpenRouter requires an API key"};

    return [cfg, prompt](std::string_view text) -> std::string {
        httplib::Client client{cfg.host};

        nlohmann::json body = {
            {"model", cfg.model},
            {"messages",
             nlohmann::json::array({{{"role", "system"}, {"content", prompt}},
                                    {{"role", "user"}, {"content", std::string{text}}}})},
            {"stream", false}};

        httplib::Headers headers;
        if (!cfg.api_key.empty())
            headers.emplace("Authorization", "Bearer " + cfg.api_key);

        auto path = chat_api_path(cfg);
        spdlog::debug("chat-api: provider={} POST {} model={} text_len={}\n  request: {}",
                      cfg.provider, path, cfg.model, text.size(), std::string{text});
        auto res = client.Post(path, headers, body.dump(), "application/json");

        if (!res)
            throw std::runtime_error{"chat-api: no response from " + cfg.host};
        if (res->status != 200)
            throw std::runtime_error{"chat-api: HTTP " + std::to_string(res->status) + " — " +
                                     res->body};

        spdlog::debug("chat-api: got response status={} body_len={}", res->status,
                      res->body.size());
        auto result = response_content(cfg, res->body);
        spdlog::debug("chat-api: response content: {}", result);
        return trim_right(std::move(result));
    };
}

Translator make_chat_api_latin_to_english_translator(const ChatApiConfig& cfg) {
    return make_chat_api_translator(cfg, translate_latin_to_english_prompt);
}

Translator make_chat_api_macron_translator(const ChatApiConfig& cfg) {
    // TODO: consider using a dictionary for looking up macrons.

    // Whenever we can, we should verify LLM output, and in this case we can superficially do so.
    // While we cannot verify if macrons were placed correctly (unless we use a dictionary),
    // we can verify that the original text hasn't changed, only macrons were added.
    // Compare word by word and place original word in brackets if pair of words mismatch.
    auto translator = make_chat_api_translator(cfg, add_macrons_to_latin_prompt);
    auto verified_postprocessor =
        [translator = std::move(translator)](std::string_view original) -> std::string {
        auto translation = translator(original);

        // Verify that original text is the same as translated, except for macrons
        auto original_split = split_words(std::string{original});
        auto translated_split = split_words(std::string{translation});

        if (original_split.words.size() != translated_split.words.size()) {
            spdlog::warn("chat-api macron: word count mismatch\n  original:   {}\n  translated: {}",
                         std::string{original}, translation);
            throw std::runtime_error("When splitting words, found mismatch between word count of "
                                     "original_words and split_words");
        }
        if (original_split.separators.size() != translated_split.separators.size()) {
            spdlog::warn(
                "chat-api macron: separator count mismatch\n  original:   {}\n  translated: {}",
                std::string{original}, translation);
            throw std::runtime_error("When splitting words, found mismatch between separator count "
                                     "of original_words and split_words");
        }
        for (size_t i = 0; i < original_split.words.size(); i++) {
            if (!compare_words(original_split.words[i], translated_split.words[i])) {
                spdlog::warn("chat-api macron: word mismatch at index {}: '{}' -> '{}'\n  "
                             "original:   {}\n  translated: {}",
                             i, original_split.words[i], translated_split.words[i],
                             std::string{original}, translation);
                translated_split.words[i] += "[" + original_split.words[i] + "]";
            }
        }
        return reconstruct(translated_split);
    };

    return verified_postprocessor;
}
