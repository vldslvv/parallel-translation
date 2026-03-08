#include "ollama.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include <spdlog/spdlog.h>

Translator make_ollama_translator(std::string model, std::string host) {
    return [model, host](std::string_view text) -> std::string {
        spdlog::debug("ollama request: host={} model={} input_bytes={}", host, model, text.size());
        httplib::Client client{host};

        nlohmann::json body = {
            {"model", model},
            {"messages",
             nlohmann::json::array({{{"role", "system"},
                                     {"content", "Translate the following Latin text to English. "
                                                 "Return only the translation, no explanation."}},
                                    {{"role", "user"}, {"content", std::string{text}}}})},
            {"stream", false}};

        auto res = client.Post("/api/chat", body.dump(), "application/json");

        if (!res)
            throw std::runtime_error{"ollama: no response from " + host};
        if (res->status != 200)
            throw std::runtime_error{"ollama: HTTP " + std::to_string(res->status) + " — " +
                                     res->body};

        auto result = nlohmann::json::parse(res->body)["message"]["content"].get<std::string>();
        auto end = result.find_last_not_of(" \t\n\r\f\v");
        if (end != std::string::npos)
            result.erase(end + 1);
        else
            result.clear();
        spdlog::debug("ollama response: output_bytes={}", result.size());
        return result;
    };
}
