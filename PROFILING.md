# Profiling

Profiling targets live in `mk/profile.mk`. Results are stored under
`build/profile-<PROFILE_BUILD>/results/`.

`PROFILE_BUILD` only names artifacts. Switch branches or commits yourself before
building each variant.

## Workload

Defaults:

```text
PROFILE_INPUT=texts/latin/de_magia.txt
PROFILE_BACKEND=stub
PROFILE_POSTPROCESSOR=morpheus
PROFILE_PARALLELISM=1
PROFILE_LOG_LEVEL=off
```

Override these variables on any profiling command.

## Run

```sh
make profile-run-time PROFILE_BUILD=current
make profile-run-cpu PROFILE_BUILD=current
make profile-run-memory PROFILE_BUILD=current
make profile-run-syscalls PROFILE_BUILD=current
```

Run the full suite:

```sh
make profile-run-suite PROFILE_BUILD=current
```

## Reports

```sh
make profile-report-time PROFILE_BUILD=current
make profile-report-cpu PROFILE_BUILD=current
make profile-report-memory PROFILE_BUILD=current
make profile-report-syscalls PROFILE_BUILD=current
```

## Compare Builds

Build each source state first:

```sh
git switch main
make profile-build PROFILE_BUILD=baseline

git switch optimize-foo
make profile-build PROFILE_BUILD=candidate
```

Run the statistical comparison:

```sh
make profile-compare-stat PROFILE_BUILD_A=baseline PROFILE_BUILD_B=candidate
```

This verifies output equality, runs both binaries in one `hyperfine` session, and
analyzes samples with a one-sided Welch test on log runtimes.

Useful knobs:

```sh
PROFILE_COMPARE_RUNS=50
PROFILE_COMPARE_WARMUP=10
PROFILE_COMPARE_MIN_SPEEDUP=3
PROFILE_COMPARE_ALPHA=0.01
```

Compare already-saved profiler artifacts for diagnosis:

```sh
make profile-compare-artifacts PROFILE_BUILD_A=baseline PROFILE_BUILD_B=candidate
```
