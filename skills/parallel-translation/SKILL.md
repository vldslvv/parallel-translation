---
name: parallel-translation
description: Use this skill when an autonomous agent needs to install, configure, and run the parallel-translation CLI to produce parallel Latin-to-English translations from text or PDF files.
---

# Parallel Translation Operator Guide

Use this skill to run the `parallel-translation` CLI. Focus on prerequisites, installation, backend health, configuration, and reliable translation jobs. Do not use this as a development guide.

## What The Tool Does

`parallel-translation` creates parallel translations from an input file to an output file.

Supported inputs:

- `.txt`
- `.pdf`

Supported outputs:

- `.txt`
- `.pdf`

The tool reads the input, splits it into translation units, translates units concurrently, optionally postprocesses the original Latin text, and writes original/translation pairs in input order.

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

If needed, update the default profile:

```sh
conan profile update settings.compiler.cppstd=23 default
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

Install the release binary to the default prefix:

```sh
make install
```

The default install prefix is `$HOME/.local`, so the installed binary is usually:

```sh
$HOME/.local/bin/parallel-translation
```

If not installing, run the built binary directly:

```sh
./build/Release/parallel-translation --help
```

## Translation Backend

The default backend is `ollama`. For real translation jobs, ensure Ollama is installed, running, and has the selected model available.

Defaults:

- Ollama host: `http://localhost:11434`
- Ollama model: `llama3`

Check backend reachability before long jobs:

```sh
curl http://localhost:11434/api/tags
```

If the selected model is missing, pull it first:

```sh
ollama pull llama3
```

For smoke tests that must not call an LLM, use:

```sh
--backend pass --postprocess none
```

The `pass` backend copies input text as the translation. This verifies paths, output format, and basic execution.

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
[ollama]
host = "http://localhost:11434"
model = "llama3"

[translation]
source_lang = "la"
target_lang = "en"
parallelism = 4

[log]
level = "warn"
```

Environment variables override the config file:

- `PT_OLLAMA_HOST`
- `PT_OLLAMA_MODEL`
- `PT_SOURCE_LANG`
- `PT_TARGET_LANG`
- `PT_LOG_LEVEL`
- `PT_PARALLELISM`

Command-line options for model, host, log level, and parallelism override config-derived values for one run.

## Running Jobs

Basic text-to-text:

```sh
parallel-translation --input input.txt --output output.txt
```

PDF to PDF:

```sh
parallel-translation --input input.pdf --output output.pdf
```

Specific model:

```sh
parallel-translation --input input.txt --output output.txt --ollama-model llama3
```

Specific host:

```sh
parallel-translation --input input.txt --output output.txt --ollama-host http://localhost:11434
```

Lower concurrency for local or unstable backends:

```sh
parallel-translation --input input.txt --output output.txt --parallelism 1
```

Disable postprocessing:

```sh
parallel-translation --input input.txt --output output.txt --postprocess none
```

Use Morpheus postprocessing with breves:

```sh
parallel-translation --input input.txt --output output.txt --postprocess morpheus --breves
```

## Autonomous Run Checklist

Before a real job:

1. Confirm the binary works: `parallel-translation --help` or `./build/Release/parallel-translation --help`.
2. Confirm the input file exists and uses `.txt` or `.pdf`.
3. Confirm the output path uses `.txt` or `.pdf`.
4. Avoid overwriting important output files unless explicitly requested.
5. If using Ollama, confirm the server is reachable and the model is available.
6. Run a smoke test with `--backend pass --postprocess none`.
7. Run the real job with conservative `--parallelism`, usually `1` to `4` for local Ollama.
8. Treat any nonzero exit code as failure and inspect logs/output.

For long jobs, write to a new output filename.

## Failure Handling

Common failure causes:

- Unsupported input or output extension
- Input file cannot be read
- Output file cannot be opened
- Unknown backend
- Ollama is unreachable
- Selected model is unavailable
- Translation backend failed
- Parallelism is above the program maximum

If a job fails:

1. Re-run with `--log-level debug`.
2. Reduce `--parallelism` to `1`.
3. Verify input and output paths.
4. Check Ollama reachability and model availability.
5. Try `--backend pass --postprocess none` to isolate file handling from LLM behavior.

## Operational Safety

- Use absolute paths from automation.
- Keep Python tools in `pipx` or a project virtual environment.
- Keep `--parallelism` conservative for local LLMs.
- Do not overwrite existing outputs without explicit permission.
- Prefer smoke tests before real translation.
- Manually inspect PDF output when possible; PDF formatting depends on document structure.
