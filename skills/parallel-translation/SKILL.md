---
name: parallel-translation
description: Use this skill when an autonomous agent needs to install, configure, and run the parallel-translation CLI to produce parallel Latin-to-English translations from text or PDF files.
---

# Parallel Translation Operator Guide

Use this skill to run the `parallel-translation` CLI. Focus on prerequisites, installation, backend health, configuration, and reliable translation jobs. Do not use this as a development guide.

Default operating rules:

- Always use Morpheus as the postprocessor unless the user explicitly requests another postprocessor or asks to disable postprocessing.
- Always translate to PDF output unless the user explicitly requests another output format. The CLI selects output format from the writer path extension, so normal jobs should use `--writer-path ...pdf`.

Before any Conan or build commands, activate the project's dedicated Conan virtual environment:

```sh
. /workspace/translations/.venv-conan/bin/activate
```

If the venv doesn't exist, create it (one-time):
```sh
python3 -m venv /workspace/translations/.venv-conan
. /workspace/translations/.venv-conan/bin/activate
pip install --upgrade pip
pip install conan
```

## What The Tool Does

`parallel-translation` creates parallel translations from an input file to an output file.

## Format Support

The CLI supports text and PDF formats. Format is selected by the file extension; there is no separate format flag.

Supported input extensions:

- `.txt`
- `.pdf`

Supported output extensions:

- `.txt`
- `.pdf`

To specify input or output format, use a path ending in the correct extension:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf
parallel-translation --reader-path input.pdf --writer-path output.pdf
```

Before running a job, verify that the input path and output path each end in `.txt` or `.pdf`. Treat missing, misspelled, or unsupported extensions as invalid job setup. Prefer `.pdf` for output unless the user asked for `.txt`.

The tool reads the input, splits it into translation units, translates units concurrently, postprocesses the original Latin text with Morpheus by default, and writes original/translation pairs in input order.

## Prerequisites

Assume a Debian-like Linux system unless the user says otherwise.

Required system capabilities:

- C++23-capable compiler, usually recent `g++` or `clang++`
- `make`
- `cmake`
- `python3`
- Python virtual environment support
- `pipx` or `pip`
- Git, if cloning the project
- General native build tooling such as linker, headers, and pkg-config support

Project C++ dependencies are managed by Conan. Prefer Conan over manually installing C++ libraries.

The active Conan profile must use C++23:

```ini
compiler.cppstd=23
```

Check it with:

```sh
conan profile show -pr default
```

If needed, update the default profile. **In Conan v2, there is no `conan profile update` command.** Edit the profile file directly:

```sh
# Find the profile path:
conan profile path  # typically ~/.conan2/profiles/default
# Then edit compiler.cppstd:
# Change: compiler.cppstd=gnu17  →  compiler.cppstd=23
```

Install Conan in an isolated Python environment. Preferred:

```sh
pipx install conan
```

`pipx` creates and manages a virtual environment for the tool, so it satisfies the isolation requirement.

If `pipx` is unavailable, create a dedicated virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install conan
```

Do not install Python dependencies into the system Python. Avoid `sudo pip`.

If the build reports missing compiler, CMake, Make, Python, virtualenv support, or system build tools, install those with the system package manager. Do not guess exact package names unless the environment makes them clear.

## Build Or Install

From the repository root, build the debug binary:

```sh
make build
```

Build the optimized release binary:

```sh
make release
```

On less-capable systems, set a lower compilation thread count to avoid memory
pressure, hangs, or failed compiler processes. The Makefile uses CMake builds,
so control build concurrency with `CMAKE_BUILD_PARALLEL_LEVEL`:

```sh
CMAKE_BUILD_PARALLEL_LEVEL=1 make build
CMAKE_BUILD_PARALLEL_LEVEL=1 make release
```

For a generic Raspberry Pi with 8 GB RAM, start with:

```sh
CMAKE_BUILD_PARALLEL_LEVEL=2 make build
CMAKE_BUILD_PARALLEL_LEVEL=2 make release
```

Use `CMAKE_BUILD_PARALLEL_LEVEL=3` or `4` only when the system has adequate
cooling and swap and remains responsive during compilation.

If compilation fails or the system becomes unresponsive, retry with
`CMAKE_BUILD_PARALLEL_LEVEL=1`.

**ARM / RPi first-build timeout warning:** When using a non-default C++ standard (e.g. C++23 on a custom profile), Conan has no pre-built binaries for ARM64 and compiles all dependencies (poppler, freetype, spdlog, pdf-writer, etc.) from source. This takes **10+ minutes** and will exceed the default 600-second foreground timeout on most agent hosts. Always start the initial build with `background=true` + `notify_on_complete=true`:

```sh
# Instead of running make directly in foreground:
#   CMAKE_BUILD_PARALLEL_LEVEL=2 make release
# Use background mode with notification:
make release  # in a terminal with background=true, notify_on_complete=true, timeout=3600
```

If the build times out, retry — Conan caches already-built packages, so subsequent attempts skip those and are much faster. Partial builds are **not** resumed from the middle; you start each Conan package from scratch on retry, but already-built packages are cached.

Install the release binary to the default prefix:

```sh
make install
```

The default install prefix is `$HOME/.local`, so the installed binary is usually:

```sh
$HOME/.local/bin/parallel-translation
```

`make install` also installs Morpheus runtime files into private application
directories. Morpheus helper binaries are not placed in `$HOME/.local/bin`;
they live under `$HOME/.local/libexec/parallel-translation`, and Morpheus data
lives under `$HOME/.local/share/parallel-translation`. The installed CLI uses
those private files and should not require the Conan cache at runtime.

If not installing, run the built binary directly:

```sh
./build/Release/parallel-translation --help
```

## Translation Backend

The default backend is `chat-api` with `--backend-chat-provider ollama`. Treat chat API
providers as external services owned outside this system. Do not install, start,
stop, restart, configure, or pull models while using this skill. Only point
`parallel-translation` at an already available endpoint and selected model.

Defaults:

- Chat provider: `ollama`
- Chat host: `http://localhost:11434`
- Chat model: `gemma3:27b`
- Postprocessor: `morpheus`
- Output format: PDF, chosen by a `.pdf` writer path
- OpenRouter default host: `https://openrouter.ai`
- OpenCode default host: `https://opencode.ai`
- OpenCode default model: `kimi-k2.6`
- Config can store `[backend.chat_api.ollama]`,
  `[backend.chat_api.openrouter]`, and `[backend.chat_api.opencode]`; the
  selected provider chooses which config is active for the run. Treat provider
  tables as provider-specific schemas, not a generic provider map.

Check local Ollama reachability before long jobs:

```sh
curl http://localhost:11434/api/tags
```

For OpenRouter and OpenCode, require an API key in the provider table, via
`PT_BACKEND_CHAT_API_KEY`, or via `--backend-chat-api-key`.
For OpenRouter, model IDs starting with `anthropic/` automatically use
OpenRouter's Anthropic Messages endpoint. Other OpenRouter models use the
normal chat-completions endpoint.
OpenCode uses the OpenAI-compatible `/zen/go/v1/chat/completions` endpoint.
Use direct API model IDs such as `kimi-k2.6`; the `opencode-go/<model-id>` form
is for OpenCode's own app config, not this API request.
If the selected model is missing or the provider is unreachable, report that the
chat API provider is not ready and ask the user or provider operator to make the
service/model available. Do not remediate it from this skill.

For smoke tests that must not call an LLM or Morpheus, use:

```sh
--backend-provider pass --postprocessor-provider none
```

The `pass` backend copies input text as the translation. This verifies paths, output format, and basic execution.

## Backend & Postprocessor Reference

| Backend | Command | Description |
|---|---|---|
| `chat-api` (default) | `--backend-provider chat-api` | Calls the configured chat API provider for real LLM translation |
| `pass` | `--backend-provider pass` | Copies input as "translation" — good for smoke tests |
| `stub` | `--backend-provider stub` | Replaces every word with `Stub` — good for pipeline testing |

| Postprocessor | Command | Description |
|---|---|---|
| `morpheus` (default) | `--postprocessor-provider morpheus` | Adds Latin macrons via vendored Morpheus engine |
| `morpheus` + breves | `--postprocessor-provider morpheus --postprocessor-breves` | Morpheus with breve marks for short vowels |
| `chat-api` | `--postprocessor-provider chat-api` | Uses the configured chat API provider for macron insertion |
| `none` | `--postprocessor-provider none` | Skip postprocessing entirely |

## CLI Reference

The binary supports `--version` / `-v` to print the current version and exit.

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
path = "output.pdf"

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
- `PT_LOG_LEVEL`
- `PT_BACKEND_PARALLELISM`
- `PT_POSTPROCESSOR_PROVIDER`
- `PT_POSTPROCESSOR_BREVES`

`PT_BACKEND_CHAT_PROVIDER` selects the active provider config.
`PT_BACKEND_CHAT_HOST`, `PT_BACKEND_CHAT_MODEL`, and
`PT_BACKEND_CHAT_API_KEY` override only that active provider. Provider tables
are provider-specific schemas; do not assume future providers will use the same
fields.

Command-line options for provider, model, host, API key, log level, and
parallelism override config-derived values for one run.
`--backend-chat-provider` selects the active provider first, then
`--backend-chat-host`, `--backend-chat-model`, and `--backend-chat-api-key`
apply to that provider.

Morpheus postprocessing uses the vendored Morpheus Conan recipe at the version
defined in `conanfile.py`. No Morpheus directory configuration is supported.

## Dependency Installation

Use the Makefile targets for builds and tests. They export the local Morpheus
recipe before installing Conan dependencies:

```sh
make build
make release
make test
```

`conanfile.py` defines `MORPHEUS_VERSION`. The Makefile reads that value,
exports `conan/recipes/morpheus` as `morpheus/<version>`, then runs
`conan install . --build=missing`. Users do not need a separate Morpheus
checkout or manual Conan export step. During `make install`, Morpheus is copied
from the Conan package into private install-prefix directories so only the main
CLI is exposed on the user's command path.

## Running Jobs

Text input to default PDF output:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf
```

PDF input to PDF output:

```sh
parallel-translation --reader-path input.pdf --writer-path output.pdf
```

Text input to PDF output:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf
```

PDF input to text output, only when text output is explicitly requested:

```sh
parallel-translation --reader-path input.pdf --writer-path output.txt
```

Specific model:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf --backend-chat-model gemma3:27
```

Specific host:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf --backend-chat-host http://localhost:11434
```

OpenRouter:

```sh
PT_BACKEND_CHAT_API_KEY=... parallel-translation --reader-path input.txt --writer-path output.pdf --backend-chat-provider openrouter --backend-chat-model google/gemma-4-31b-it
```

OpenRouter Anthropic Messages:

```sh
PT_BACKEND_CHAT_API_KEY=... parallel-translation --reader-path input.txt --writer-path output.pdf --backend-chat-provider openrouter --backend-chat-model anthropic/claude-sonnet-4
```

OpenCode:

```sh
PT_BACKEND_CHAT_API_KEY=... parallel-translation --reader-path input.txt --writer-path output.pdf --backend-chat-provider opencode --backend-chat-model kimi-k2.6
```

Set concurrency. Use `--backend-parallelism 1` unless the user explicitly
instructs a different value:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf --backend-parallelism 1
```

Disable postprocessing, only when explicitly requested or isolating file handling:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf --postprocessor-provider none
```

Use Morpheus postprocessing with breves:

```sh
parallel-translation --reader-path input.txt --writer-path output.pdf --postprocessor-provider morpheus --postprocessor-breves
```

## Autonomous Run Checklist

Before a real job:

1. Confirm the binary works: `parallel-translation --help` or `./build/Release/parallel-translation --help`.
2. Confirm the input file exists.
3. Confirm the input file extension is `.txt` or `.pdf`; this selects the input format.
4. Confirm the output path extension is `.pdf` unless the user explicitly requested `.txt`; this selects the output format.
5. Avoid overwriting important output files unless explicitly requested.
6. If using a chat API provider, confirm the external server is reachable, the model is available, and hosted providers have an API key.
7. Run a smoke test with `--backend-provider pass --postprocessor-provider none`.
8. Run the real job with Morpheus postprocessing and `--backend-parallelism 1` unless the user explicitly instructs a different value.
9. Treat any nonzero exit code as failure and inspect logs/output.

For long jobs, write to a new output filename.

## Failure Handling

Common failure causes:

- Unsupported input or output extension
- Input file cannot be read
- Output file cannot be opened
- Unknown backend
- Chat API provider is unreachable
- Selected model is unavailable
- Translation backend failed
- Parallelism is above the program maximum

If a job fails:

1. Re-run with `--log-level debug`.
2. Reduce `--backend-parallelism` to `1`.
3. Verify input and output paths.
4. Check chat API provider reachability, model availability, and API key configuration.
5. Try `--backend-provider pass --postprocessor-provider none` to isolate file handling from LLM behavior.

## Exit Codes

The program uses these exit codes — useful for autonomous agents interpreting failures:

| Code | Name | Meaning |
|---|---|---|
| `0` | `success` | Translation completed successfully |
| `1` | `runtime_error` | Translation or postprocessing failed mid-run |
| `2` | `usage_error` | Invalid CLI arguments (unknown backend, unsupported extension, parallelism too high) |
| `3` | `input_error` | Input file could not be read or parsed |
| `4` | `output_error` | Output file could not be opened or written |
| `5` | `backend_unavailable` | Translation backend is unreachable |

A nonzero exit code should always be treated as failure — inspect stderr logs with `--log-level debug` for details.

## Operational Safety

- Use absolute paths from automation.
- Keep Python tools in `pipx` or a project virtual environment.
- Use Morpheus postprocessing for real translation jobs unless explicitly told otherwise.
- Use PDF output for real translation jobs unless explicitly told otherwise.
- Use `--backend-parallelism 1` for chat-api-backed jobs unless instructed otherwise.
- Do not overwrite existing outputs without explicit permission.
- Prefer smoke tests before real translation.
- Manually inspect PDF output when possible; PDF formatting depends on document structure.
