#include "ollama.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

Translator make_ollama_translator(std::string model, std::string host) {
    return [model, host](std::string_view text) -> std::string {
        httplib::Client client{host};

        nlohmann::json body = {
            {"model", model},
            {"messages", nlohmann::json::array({
                {{"role", "system"}, {"content", "Translate the following Latin text to English. Return only the translation, no explanation."}},
                {{"role", "user"},   {"content", std::string{text}}}
            })},
            {"stream", false}
        };

        auto res = client.Post("/api/chat", body.dump(), "application/json");

        if (!res)
            throw std::runtime_error{"ollama: no response from " + host};
        if (res->status != 200)
            throw std::runtime_error{"ollama: HTTP " + std::to_string(res->status) + " — " + res->body};

        return nlohmann::json::parse(res->body)["message"]["content"].get<std::string>();
    };
}
