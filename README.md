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

Morpheus postprocessing uses the vendored Morpheus Conan recipe. The Makefile
exports that recipe at the version defined in `conanfile.py` before installing
dependencies, so no separate Morpheus checkout or directory configuration is
required.

## Examples

```sh
parallel-translation --input input.txt --output output.txt
parallel-translation --input input.txt --output output.txt --postprocess none
```
