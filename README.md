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

[morpheus]
dir = "/home/user/ccode/morpheus"

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
- `PT_MORPHEUS_DIR`
- `PT_SOURCE_LANG`
- `PT_TARGET_LANG`
- `PT_LOG_LEVEL`
- `PT_PARALLELISM`

Command-line options for model, host, Morpheus directory, log level, and parallelism override config-derived values for one run.

Morpheus postprocessing requires `dir` to be set through `[morpheus].dir`,
`PT_MORPHEUS_DIR`, or `--morpheus-dir`. Use `--postprocess none` when Morpheus
is not configured.

## Examples

```sh
parallel-translation --input input.txt --output output.txt --morpheus-dir /home/user/ccode/morpheus
parallel-translation --input input.txt --output output.txt --postprocess none
```
