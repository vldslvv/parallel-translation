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
parallel-translation --input input.txt --output output.txt
parallel-translation --input input.txt --output output.txt --postprocess none
```
