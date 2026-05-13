---
name: parallel-translation
description: Use this skill when an autonomous agent needs to install, configure, and run the parallel-translation CLI to produce parallel Latin-to-English translations from text or PDF files.
---

# Parallel Translation Operator Guide

Use this skill to run the `parallel-translation` CLI. Focus on prerequisites, installation, backend health, configuration, and reliable translation jobs. Do not use this as a development guide.

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
parallel-translation --input input.txt --output output.pdf
parallel-translation --input input.pdf --output output.txt
```

Before running a job, verify that the input path and output path each end in `.txt` or `.pdf`. Treat missing, misspelled, or unsupported extensions as invalid job setup.

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

The default backend is `ollama`. Treat Ollama as an external service owned
outside this system. Do not install, start, stop, restart, configure, or pull
models with Ollama while using this skill. Only point `parallel-translation` at
an already available Ollama endpoint and selected model.

Defaults:

- Ollama host: `http://localhost:11434`
- Ollama model: `gemma3:27b`

Check backend reachability before long jobs:

```sh
curl http://localhost:11434/api/tags
```

If the selected model is missing or the server is unreachable, report that
Ollama is not ready and ask the user or the external Ollama operator to make the
service/model available. Do not remediate it from this skill.

For smoke tests that must not call an LLM or Morpheus, use:

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
model = "gemma3:27b"

[translation]
source_lang = "la"
target_lang = "en"
parallelism = 1

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
checkout or manual Conan export step.

## Running Jobs

Text input to text output:

```sh
parallel-translation --input input.txt --output output.txt
```

PDF input to PDF output:

```sh
parallel-translation --input input.pdf --output output.pdf
```

Text input to PDF output:

```sh
parallel-translation --input input.txt --output output.pdf
```

PDF input to text output:

```sh
parallel-translation --input input.pdf --output output.txt
```

Specific model:

```sh
parallel-translation --input input.txt --output output.txt --ollama-model gemma3:27
```

Specific host:

```sh
parallel-translation --input input.txt --output output.txt --ollama-host http://localhost:11434
```

Set concurrency. Use `--parallelism 1` unless the user explicitly instructs a
different value:

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
2. Confirm the input file exists.
3. Confirm the input file extension is `.txt` or `.pdf`; this selects the input format.
4. Confirm the output path extension is `.txt` or `.pdf`; this selects the output format.
5. Avoid overwriting important output files unless explicitly requested.
6. If using Ollama, confirm the external server is reachable and the model is available.
7. Run a smoke test with `--backend pass --postprocess none`.
8. Run the real job with `--parallelism 1` unless the user explicitly instructs a different value.
9. Treat any nonzero exit code as failure and inspect logs/output.

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
- Use `--parallelism 1` for Ollama-backed jobs unless instructed otherwise.
- Do not overwrite existing outputs without explicit permission.
- Prefer smoke tests before real translation.
- Manually inspect PDF output when possible; PDF formatting depends on document structure.
