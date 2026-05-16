# parallel-translation
A CLI tool to create parallel translations

## Configuration

Config file location:

```sh
$XDG_CONFIG_HOME/parallel-translation/config.toml
```

If `XDG_CONFIG_HOME` is unset:

```sh
$HOME/.config/parallel-translation/config.toml
```

Example config:

```toml
[reader]
path = "input.txt"

[postprocessing]
provider = "morpheus"
breves = false

[backend]
provider = "chat-api"
source_lang = "la"
target_lang = "en"
parallelism = 1

[backend.chat_api]
provider = "ollama"

[backend.chat_api.ollama]
host = "http://localhost:11434"
model = "gemma3:27b"
api_key = ""

[backend.chat_api.openrouter]
host = "https://openrouter.ai"
model = "google/gemma-4-31b-it"
api_key = ""

[backend.chat_api.opencode]
host = "https://opencode.ai"
model = "kimi-k2.6"
api_key = ""

[writer]
path = "output.txt"

[log]
level = "warn"
```

Environment variables override the config file:

- `PT_READER_PATH`
- `PT_WRITER_PATH`
- `PT_BACKEND_PROVIDER`
- `PT_BACKEND_CHAT_PROVIDER`
- `PT_BACKEND_CHAT_HOST`
- `PT_BACKEND_CHAT_MODEL`
- `PT_BACKEND_CHAT_API_KEY`
- `PT_BACKEND_SOURCE_LANG`
- `PT_BACKEND_TARGET_LANG`
- `PT_BACKEND_PARALLELISM`
- `PT_POSTPROCESSOR_PROVIDER`
- `PT_POSTPROCESSOR_BREVES`
- `PT_LOG_LEVEL`

`PT_BACKEND_CHAT_PROVIDER` chooses which chat API provider config is active.
`PT_BACKEND_CHAT_HOST`, `PT_BACKEND_CHAT_MODEL`, and
`PT_BACKEND_CHAT_API_KEY` override only that active provider.
The app derives the provider's API style and endpoint path internally; users
only configure host, model, and API key.

Command-line options for provider, model, host, API key, log level, and
parallelism override config-derived values for one run.
`--backend-chat-provider` selects the active provider first, then
`--backend-chat-host`, `--backend-chat-model`, and `--backend-chat-api-key`
apply to that provider.

OpenRouter can be selected without changing `config.toml`:

```sh
PT_BACKEND_CHAT_API_KEY=... parallel-translation \
  --reader-path input.txt \
  --writer-path output.txt \
  --backend-chat-provider openrouter \
  --backend-chat-model google/gemma-4-31b-it
```

The default OpenRouter host is `https://openrouter.ai`. Override it with
`--backend-chat-host` or `PT_BACKEND_CHAT_HOST` only when using a compatible
proxy.

OpenCode Go can also be selected without changing `config.toml`:

```sh
PT_BACKEND_CHAT_API_KEY=... parallel-translation \
  --reader-path input.txt \
  --writer-path output.txt \
  --backend-chat-provider opencode \
  --backend-chat-model kimi-k2.6
```

The default OpenCode host is `https://opencode.ai`, using the
`/zen/go/v1/chat/completions` OpenAI-compatible endpoint. Use direct API model
IDs such as `kimi-k2.6`; the `opencode-go/<model-id>` form is for OpenCode's
own app config, not this API request.

Morpheus postprocessing uses the vendored Morpheus Conan recipe. The Makefile
exports that recipe at the version defined in `conanfile.py` before installing
dependencies, so no separate Morpheus checkout or directory configuration is
required.

## Dependencies

The Makefile handles Morpheus installation through Conan:

```sh
make build
```

`conanfile.py` defines `MORPHEUS_VERSION`. The Makefile reads that value,
exports `conan/recipes/morpheus` as `morpheus/<version>`, then runs
`conan install . --build=missing`. This creates the local Morpheus package
automatically when it is not already present.

The same bootstrap happens for `make release` and `make test`.

`make install` copies the Morpheus runtime files from the Conan package into
private application directories under the install prefix. With the default
prefix, only `~/.local/bin/parallel-translation` is added to the normal command
path; Morpheus helpers are hidden under `~/.local/libexec/parallel-translation`
and data is installed under `~/.local/share/parallel-translation`. The installed
binary uses those private files and does not need the Conan cache at runtime.

## Examples

```sh
parallel-translation --reader-path input.txt --writer-path output.txt
parallel-translation --reader-path input.txt --writer-path output.txt --postprocessor-provider none
parallel-translation --reader-path input.txt --writer-path output.txt --backend-chat-provider openrouter --backend-chat-model google/gemma-4-31b-it
parallel-translation --reader-path input.txt --writer-path output.txt --backend-chat-provider opencode --backend-chat-model kimi-k2.6
```
