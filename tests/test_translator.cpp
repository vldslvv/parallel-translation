#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "app.hpp"
#include "common/exit_codes.hpp"
#include "config.hpp"
#include "test_helpers.hpp"
#include "translators/ollama.hpp"

static const char* INPUT = ASSETS_DIR "/latin_example.txt";
static const char* OUTPUT = ASSETS_DIR "/latin_example_out.txt";

class EnvVar {
  public:
    EnvVar(const char* name, const char* value) : name_{name} {
        if (const char* old = std::getenv(name))
            old_ = old;
        if (value)
            setenv(name, value, 1);
        else
            unsetenv(name);
    }

    ~EnvVar() {
        if (old_.empty())
            unsetenv(name_.c_str());
        else
            setenv(name_.c_str(), old_.c_str(), 1);
    }

  private:
    std::string name_;
    std::string old_;
};

class TestServer {
  public:
    TestServer() {
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ < 0)
            throw std::runtime_error{"failed to bind test server"};
    }

    ~TestServer() {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    httplib::Server& server() { return server_; }

    void start() {
        thread_ = std::thread{[this] { server_.listen_after_bind(); }};
    }

    std::string host() const { return "http://127.0.0.1:" + std::to_string(port_); }

  private:
    httplib::Server server_;
    int port_;
    std::thread thread_;
};

static std::filesystem::path make_config_dir() {
    auto dir = std::filesystem::temp_directory_path() /
               ("parallel-translation-test-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir / "parallel-translation");
    return dir;
}

TEST_CASE("translates input file to output file", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub", "--postprocess", "none",
                          "-i",  INPUT,       "-o",   OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);

    auto out = read_file(OUTPUT);
    CHECK(!out.empty());
    // stub replaces every word with "Stub", separators are preserved
    CHECK(out.contains("Stub"));

    std::remove(OUTPUT);
}

TEST_CASE("loads provider-specific chat api config from toml and env", "[config]") {
    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"openrouter\"\n"
              "\n"
              "[ollama]\n"
              "host = \"http://ollama.invalid\"\n"
              "model = \"llama3\"\n"
              "api_key = \"ollama-key\"\n"
              "\n"
              "[openrouter]\n"
              "host = \"https://openrouter.invalid\"\n"
              "model = \"anthropic/claude-sonnet-4\"\n"
              "api_key = \"from-file\"\n"
              "\n"
              "[translation]\n"
              "parallelism = 3\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", nullptr};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", "openai/gpt-4o-mini"};
    EnvVar key{"PT_CHAT_API_KEY", "from-env"};

    auto cfg = load_config();
    const auto chat = selected_chat_api_config(cfg);

    CHECK(cfg.chat_api_provider == "openrouter");
    CHECK(chat.provider == "openrouter");
    CHECK(chat.host == "https://openrouter.invalid");
    CHECK(chat.model == "openai/gpt-4o-mini");
    CHECK(chat.api_key == "from-env");
    CHECK(cfg.ollama.host == "http://ollama.invalid");
    CHECK(cfg.ollama.model == "llama3");
    CHECK(cfg.ollama.api_key == "ollama-key");
    CHECK(cfg.parallelism == 3);

    std::filesystem::remove_all(config_dir);
}

TEST_CASE("chat provider env switches selected provider without changing provider tables",
          "[config]") {
    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"ollama\"\n"
              "\n"
              "[ollama]\n"
              "host = \"http://ollama.invalid\"\n"
              "model = \"llama3\"\n"
              "\n"
              "[openrouter]\n"
              "host = \"https://openrouter.invalid\"\n"
              "model = \"google/gemma-4-31b-it\"\n"
              "api_key = \"from-file\"\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", "openrouter"};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", nullptr};
    EnvVar key{"PT_CHAT_API_KEY", nullptr};

    auto cfg = load_config();
    const auto chat = selected_chat_api_config(cfg);

    CHECK(cfg.chat_api_provider == "openrouter");
    CHECK(chat.host == "https://openrouter.invalid");
    CHECK(chat.model == "google/gemma-4-31b-it");
    CHECK(chat.api_key == "from-file");
    CHECK(cfg.ollama.host == "http://ollama.invalid");

    std::filesystem::remove_all(config_dir);
}

TEST_CASE("defaults chat api host by provider", "[config]") {
    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"openrouter\"\n"
              "\n"
              "[openrouter]\n"
              "model = \"google/gemma-4-31b-it\"\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", nullptr};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", nullptr};
    EnvVar key{"PT_CHAT_API_KEY", nullptr};

    auto cfg = load_config();
    const auto chat = selected_chat_api_config(cfg);

    CHECK(cfg.chat_api_provider == "openrouter");
    CHECK(chat.host == "https://openrouter.ai");

    std::filesystem::remove_all(config_dir);
}

TEST_CASE("legacy chat api fields are not provider config", "[config]") {
    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"openrouter\"\n"
              "host = \"https://legacy.invalid\"\n"
              "model = \"legacy-model\"\n"
              "api_key = \"legacy-key\"\n"
              "\n"
              "[openrouter]\n"
              "model = \"google/gemma-4-31b-it\"\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", nullptr};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", nullptr};
    EnvVar key{"PT_CHAT_API_KEY", nullptr};

    auto cfg = load_config();
    const auto chat = selected_chat_api_config(cfg);

    CHECK(chat.provider == "openrouter");
    CHECK(chat.host == "https://openrouter.ai");
    CHECK(chat.model == "google/gemma-4-31b-it");
    CHECK(chat.api_key.empty());

    std::filesystem::remove_all(config_dir);
}

TEST_CASE("ollama selected config preserves api key", "[config]") {
    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"ollama\"\n"
              "\n"
              "[ollama]\n"
              "host = \"http://ollama.invalid\"\n"
              "model = \"llama3\"\n"
              "api_key = \"ollama-secret\"\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", nullptr};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", nullptr};
    EnvVar key{"PT_CHAT_API_KEY", nullptr};

    auto cfg = load_config();
    const auto chat = selected_chat_api_config(cfg);

    CHECK(chat.provider == "ollama");
    CHECK(chat.host == "http://ollama.invalid");
    CHECK(chat.model == "llama3");
    CHECK(chat.api_key == "ollama-secret");

    std::filesystem::remove_all(config_dir);
}

TEST_CASE("unknown chat provider is rejected without implicit config", "[config]") {
    Config cfg;
    cfg.chat_api_provider = "future-provider";

    try {
        (void)selected_chat_api_config(cfg);
        FAIL("expected unknown provider to throw");
    } catch (const std::runtime_error& e) {
        CHECK(std::string{e.what()} == "chat-api: unknown provider: future-provider");
    }
}

TEST_CASE("chat api ollama provider posts to ollama endpoint", "[translator]") {
    TestServer test_server;
    std::atomic_bool called{false};

    test_server.server().Post(
        "/api/chat", [&](const httplib::Request& req, httplib::Response& res) {
            called = true;
            auto body = nlohmann::json::parse(req.body);
            CHECK(body["model"] == "llama3");
            CHECK(body["stream"] == false);
            CHECK(body["messages"][0]["role"] == "system");
            CHECK(body["messages"][1]["content"] == "Salve");
            res.set_content(R"({"message":{"content":"Hello  "}})", "application/json");
        });
    test_server.start();

    ChatApiConfig cfg;
    cfg.provider = "ollama";
    cfg.host = test_server.host();
    cfg.model = "llama3";

    auto translator = make_chat_api_latin_to_english_translator(cfg);

    CHECK(translator("Salve") == "Hello");
    CHECK(called);
}

TEST_CASE("chat api openrouter provider posts to chat completions with bearer auth",
          "[translator]") {
    TestServer test_server;
    std::atomic_bool called{false};

    test_server.server().Post(
        "/api/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
            called = true;
            CHECK(req.get_header_value("Authorization") == "Bearer secret-key");
            auto body = nlohmann::json::parse(req.body);
            CHECK(body["model"] == "google/gemma-4-31b-it");
            CHECK(body["stream"] == false);
            CHECK(body["messages"][1]["content"] == "Salve");
            res.set_content(R"({"choices":[{"message":{"content":"Hello"}}]})", "application/json");
        });
    test_server.start();

    ChatApiConfig cfg;
    cfg.provider = "openrouter";
    cfg.host = test_server.host();
    cfg.model = "google/gemma-4-31b-it";
    cfg.api_key = "secret-key";

    auto translator = make_chat_api_latin_to_english_translator(cfg);

    CHECK(translator("Salve") == "Hello");
    CHECK(called);
}

TEST_CASE("chat api openrouter requires an api key", "[translator]") {
    ChatApiConfig cfg;
    cfg.provider = "openrouter";
    cfg.host = "https://openrouter.ai";
    cfg.model = "google/gemma-4-31b-it";

    try {
        (void)make_chat_api_latin_to_english_translator(cfg);
        FAIL("expected missing API key to throw");
    } catch (const std::runtime_error& e) {
        CHECK(std::string{e.what()} == "chat-api: OpenRouter requires an API key");
    }
}

TEST_CASE("cli chat provider selects matching provider config before overrides", "[integration]") {
    TestServer test_server;
    std::atomic_bool called{false};

    test_server.server().Post(
        "/api/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
            called = true;
            CHECK(req.get_header_value("Authorization") == "Bearer cli-key");
            auto body = nlohmann::json::parse(req.body);
            CHECK(body["model"] == "cli-model");
            res.set_content(R"({"choices":[{"message":{"content":"Hello"}}]})", "application/json");
        });
    test_server.start();

    auto config_dir = make_config_dir();
    std::ofstream config{config_dir / "parallel-translation" / "config.toml"};
    config << "[chat_api]\n"
              "provider = \"ollama\"\n"
              "\n"
              "[ollama]\n"
              "host = \"http://ollama.invalid\"\n"
              "model = \"llama3\"\n"
              "\n"
              "[openrouter]\n"
              "host = \"https://openrouter.invalid\"\n"
              "model = \"google/gemma-4-31b-it\"\n"
              "api_key = \"from-file\"\n";
    config.close();

    EnvVar xdg{"XDG_CONFIG_HOME", config_dir.c_str()};
    EnvVar provider{"PT_CHAT_PROVIDER", nullptr};
    EnvVar host{"PT_CHAT_HOST", nullptr};
    EnvVar model{"PT_CHAT_MODEL", nullptr};
    EnvVar key{"PT_CHAT_API_KEY", nullptr};

    auto server_host = test_server.host();
    const char* argv[] = {"app",
                          "--backend",
                          "chat-api",
                          "--postprocess",
                          "none",
                          "--parallelism",
                          "1",
                          "--chat-provider",
                          "openrouter",
                          "--chat-host",
                          server_host.c_str(),
                          "--chat-model",
                          "cli-model",
                          "--chat-api-key",
                          "cli-key",
                          "-i",
                          INPUT,
                          "-o",
                          OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == 0);
    CHECK(called);

    std::remove(OUTPUT);
    std::filesystem::remove_all(config_dir);
}

TEST_CASE("errors on non-existent input file", "[integration]") {
    const char* argv[] = {"app",
                          "--backend",
                          "stub",
                          "--postprocess",
                          "none",
                          "-i",
                          ASSETS_DIR // NOLINT(bugprone-suspicious-missing-comma)
                          "/nonexistent.txt",
                          "-o",
                          OUTPUT};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == exit_code::input_error);
}

TEST_CASE("errors when parallelism exceeds semaphore capacity", "[integration]") {
    const char* argv[] = {"app", "--backend", "stub", "--postprocess", "none", "-i",
                          INPUT, "-o",        OUTPUT, "--parallelism", "1025"};

    CHECK(run(std::size(argv), const_cast<char**>(argv)) == exit_code::usage_error);

    std::remove(OUTPUT);
}
