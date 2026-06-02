##@ Profiling

PROFILE_BUILD_TYPE := RelWithDebInfo
PROFILE_BUILD ?=
PROFILE_BUILD_A ?=
PROFILE_BUILD_B ?=
PROFILE_BUILD_SUFFIX := $(if $(PROFILE_BUILD),-$(PROFILE_BUILD),)
PROFILE_BUILD_LABEL := $(if $(PROFILE_BUILD),$(PROFILE_BUILD),<default>)
PROFILE_BUILD_ARG := $(if $(PROFILE_BUILD), PROFILE_BUILD=$(PROFILE_BUILD),)
PROFILE_ROOT := $(BUILD_DIR)/profile$(PROFILE_BUILD_SUFFIX)
PROFILE_DIR := $(PROFILE_ROOT)/cmake
PROFILE_RESULTS_ROOT := $(PROFILE_ROOT)/results
PROFILE_TIME_DIR := $(PROFILE_RESULTS_ROOT)/time
PROFILE_PERF_DIR := $(PROFILE_RESULTS_ROOT)/perf
PROFILE_MEMORY_DIR := $(PROFILE_RESULTS_ROOT)/memory
PROFILE_SYSCALLS_DIR := $(PROFILE_RESULTS_ROOT)/syscalls
PROFILE_GENERATORS_DIR := $(PROFILE_DIR)/build/$(PROFILE_BUILD_TYPE)/generators
PROFILE_COMPARE_A_ROOT := $(BUILD_DIR)/profile-$(PROFILE_BUILD_A)
PROFILE_COMPARE_B_ROOT := $(BUILD_DIR)/profile-$(PROFILE_BUILD_B)
PROFILE_COMPARE_A_BIN := $(PROFILE_COMPARE_A_ROOT)/cmake/parallel-translation
PROFILE_COMPARE_B_BIN := $(PROFILE_COMPARE_B_ROOT)/cmake/parallel-translation
PROFILE_COMPARE_A_RESULTS_ROOT := $(PROFILE_COMPARE_A_ROOT)/results
PROFILE_COMPARE_B_RESULTS_ROOT := $(PROFILE_COMPARE_B_ROOT)/results
PROFILE_COMPARE_A_TIME_OUTPUT := $(PROFILE_COMPARE_A_RESULTS_ROOT)/time/output.txt
PROFILE_COMPARE_B_TIME_OUTPUT := $(PROFILE_COMPARE_B_RESULTS_ROOT)/time/output.txt
PROFILE_COMPARE_A_HYPERFINE_JSON := $(PROFILE_COMPARE_A_RESULTS_ROOT)/time/hyperfine.json
PROFILE_COMPARE_B_HYPERFINE_JSON := $(PROFILE_COMPARE_B_RESULTS_ROOT)/time/hyperfine.json
PROFILE_COMPARE_A_PERF_DATA := $(PROFILE_COMPARE_A_RESULTS_ROOT)/perf/perf.data
PROFILE_COMPARE_B_PERF_DATA := $(PROFILE_COMPARE_B_RESULTS_ROOT)/perf/perf.data
PROFILE_COMPARE_A_HEAPTRACK_DATA := $(PROFILE_COMPARE_A_RESULTS_ROOT)/memory/heaptrack.data.zst
PROFILE_COMPARE_B_HEAPTRACK_DATA := $(PROFILE_COMPARE_B_RESULTS_ROOT)/memory/heaptrack.data.zst
PROFILE_COMPARE_A_STRACE_SUMMARY := $(PROFILE_COMPARE_A_RESULTS_ROOT)/syscalls/strace-summary.txt
PROFILE_COMPARE_B_STRACE_SUMMARY := $(PROFILE_COMPARE_B_RESULTS_ROOT)/syscalls/strace-summary.txt

PROFILE_INPUT := texts/latin/de_magia.txt
# PROFILE_INPUT := texts/latin/de_magia_repeated_for_profiling.txt
PROFILE_TIME_OUTPUT := $(PROFILE_TIME_DIR)/output.txt
PROFILE_PERF_OUTPUT := $(PROFILE_PERF_DIR)/output.txt
PROFILE_MEMORY_OUTPUT := $(PROFILE_MEMORY_DIR)/output.txt
PROFILE_SYSCALLS_OUTPUT := $(PROFILE_SYSCALLS_DIR)/output.txt
PROFILE_PARALLELISM := 1
PROFILE_BACKEND := stub
PROFILE_POSTPROCESSOR := morpheus
PROFILE_LOG_LEVEL := off
PROFILE_COMPARE_RUNS ?= 30
PROFILE_COMPARE_WARMUP ?= 5
PROFILE_COMPARE_MIN_SPEEDUP ?= 1
PROFILE_COMPARE_ALPHA ?= 0.05
PROFILE_COMPARE_HYPERFINE_ARGS ?=
PROFILE_COMPARE_STAT_ROOT := $(BUILD_DIR)/profile-compare-stat/$(PROFILE_BUILD_A)-vs-$(PROFILE_BUILD_B)
PROFILE_COMPARE_STAT_JSON := $(PROFILE_COMPARE_STAT_ROOT)/hyperfine.json
PROFILE_COMPARE_STAT_ANALYSIS := $(PROFILE_COMPARE_STAT_ROOT)/analysis.txt
PROFILE_COMPARE_STAT_A_OUTPUT := $(PROFILE_COMPARE_STAT_ROOT)/output-a.txt
PROFILE_COMPARE_STAT_B_OUTPUT := $(PROFILE_COMPARE_STAT_ROOT)/output-b.txt

PROFILE_BIN := $(PROFILE_DIR)/parallel-translation
# Use PROFILE_BUILD=<name> to keep separate profiling builds and results, for
# example build/profile-cached-paths/cmake and
# build/profile-cached-paths/results/perf/perf.data.
PERF_DATA := $(PROFILE_PERF_DIR)/perf.data
HYPERFINE_JSON := $(PROFILE_TIME_DIR)/hyperfine.json
HEAPTRACK_OUTPUT := $(PROFILE_MEMORY_DIR)/heaptrack.data
HEAPTRACK_DATA := $(HEAPTRACK_OUTPUT).zst
STRACE_SUMMARY := $(PROFILE_SYSCALLS_DIR)/strace-summary.txt
# Do not profile forked helpers such as Morpheus; keep CPU samples scoped to
# the parallel-translation process we are optimizing.
PROFILE_PERF_RECORD_ARGS := --call-graph dwarf --no-inherit
profile_command = $(PROFILE_BIN) \
	--reader-path $(PROFILE_INPUT) \
	--writer-path $(1) \
	--backend-provider $(PROFILE_BACKEND) \
	--postprocessor-provider $(PROFILE_POSTPROCESSOR) \
	--backend-parallelism $(PROFILE_PARALLELISM) \
	--log-level $(PROFILE_LOG_LEVEL)
profile_command_for = $(1) \
	--reader-path $(PROFILE_INPUT) \
	--writer-path $(2) \
	--backend-provider $(PROFILE_BACKEND) \
	--postprocessor-provider $(PROFILE_POSTPROCESSOR) \
	--backend-parallelism $(PROFILE_PARALLELISM) \
	--log-level $(PROFILE_LOG_LEVEL)

define clear_profile_results_if_needed
	@if [ -d "$(1)" ] && [ -n "$$(find "$(1)" -mindepth 1 -maxdepth 1 -print -quit)" ]; then \
		printf '%s already contains profiling data. Clear data for PROFILE_BUILD=%s? [y/N] ' "$(1)" "$(PROFILE_BUILD_LABEL)"; \
		read answer; \
		case "$$answer" in \
			y|Y) rm -rf "$(1)" ;; \
			*) echo "aborted; run 'make profile-clean$(PROFILE_BUILD_ARG)' to clear it."; exit 1 ;; \
		esac; \
	fi
endef

define require_profile_compare_builds
	@if [ -z "$(PROFILE_BUILD_A)" ] || [ -z "$(PROFILE_BUILD_B)" ]; then \
		echo "PROFILE_BUILD_A and PROFILE_BUILD_B are required."; \
		echo "example: make $(1) PROFILE_BUILD_A=before-loop PROFILE_BUILD_B=after-loop"; \
		exit 1; \
	fi
endef

define require_profile_compare_file
	@if [ ! -f "$(1)" ]; then \
		echo "missing required profiling result: $(1)"; \
		exit 1; \
	fi
endef

define clear_profile_compare_stat_results_if_needed
	@if [ -d "$(PROFILE_COMPARE_STAT_ROOT)" ] && [ -n "$$(find "$(PROFILE_COMPARE_STAT_ROOT)" -mindepth 1 -maxdepth 1 -print -quit)" ]; then \
		printf '%s already contains benchmark comparison data. Clear it? [y/N] ' "$(PROFILE_COMPARE_STAT_ROOT)"; \
		read answer; \
		case "$$answer" in \
			y|Y) rm -rf "$(PROFILE_COMPARE_STAT_ROOT)" ;; \
			*) echo "aborted; remove $(PROFILE_COMPARE_STAT_ROOT) to clear it."; exit 1 ;; \
		esac; \
	fi
endef

.PHONY: profile-build
profile-build: conan-profile morpheus-recipe ## Build optimized binary with debug symbols
	$(CONAN) install . --build=missing -of $(PROFILE_DIR) -s build_type=$(PROFILE_BUILD_TYPE) -s compiler.cppstd=$(CONAN_CPPSTD)
	cmake -B $(PROFILE_DIR) \
		-DCMAKE_TOOLCHAIN_FILE=$(PROFILE_GENERATORS_DIR)/conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=$(PROFILE_BUILD_TYPE) \
		-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g -fno-omit-frame-pointer"
	cmake --build $(PROFILE_DIR)

.PHONY: profile-run-time
profile-run-time: profile-build ## Run hyperfine timing profile for selected PROFILE_BUILD
	$(call clear_profile_results_if_needed,$(PROFILE_TIME_DIR))
	@mkdir -p $(PROFILE_TIME_DIR)
	hyperfine --warmup 2 --runs 10 --export-json $(HYPERFINE_JSON) '$(call profile_command,$(PROFILE_TIME_OUTPUT))'

.PHONY: profile-report-time
profile-report-time: ## Print saved hyperfine timing report for selected PROFILE_BUILD
	$(call require_profile_compare_file,$(HYPERFINE_JSON))
	@jq -r '.results[0] | "mean_s\t\(.mean)\nstddev_s\t\(.stddev)\nmedian_s\t\(.median)\nmin_s\t\(.min)\nmax_s\t\(.max)"' "$(HYPERFINE_JSON)"

.PHONY: profile-run-cpu
profile-run-cpu: profile-build ## Run perf CPU profile for selected PROFILE_BUILD
	$(call clear_profile_results_if_needed,$(PROFILE_PERF_DIR))
	@mkdir -p $(PROFILE_PERF_DIR)
	perf record $(PROFILE_PERF_RECORD_ARGS) --output $(PERF_DATA) -- $(call profile_command,$(PROFILE_PERF_OUTPUT))

.PHONY: profile-report-cpu
profile-report-cpu: ## Open saved perf CPU report for selected PROFILE_BUILD
	$(call require_profile_compare_file,$(PERF_DATA))
	perf report --input $(PERF_DATA)

.PHONY: profile-run-memory
profile-run-memory: profile-build ## Run heaptrack memory profile for selected PROFILE_BUILD
	$(call clear_profile_results_if_needed,$(PROFILE_MEMORY_DIR))
	@mkdir -p $(PROFILE_MEMORY_DIR)
	heaptrack --record-only --output $(HEAPTRACK_OUTPUT) $(call profile_command,$(PROFILE_MEMORY_OUTPUT))

.PHONY: profile-report-memory
profile-report-memory: ## Print saved heaptrack memory report for selected PROFILE_BUILD
	$(call require_profile_compare_file,$(HEAPTRACK_DATA))
	heaptrack_print "$(HEAPTRACK_DATA)" | sed -n '1,80p'

.PHONY: profile-run-syscalls
profile-run-syscalls: profile-build ## Run strace syscall profile for selected PROFILE_BUILD
	$(call clear_profile_results_if_needed,$(PROFILE_SYSCALLS_DIR))
	@mkdir -p $(PROFILE_SYSCALLS_DIR)
	strace -f -c -o $(STRACE_SUMMARY) $(call profile_command,$(PROFILE_SYSCALLS_OUTPUT))

.PHONY: profile-report-syscalls
profile-report-syscalls: ## Print saved strace syscall report for selected PROFILE_BUILD
	$(call require_profile_compare_file,$(STRACE_SUMMARY))
	@cat "$(STRACE_SUMMARY)"

.PHONY: profile-run-suite
profile-run-suite: profile-run-time profile-run-cpu profile-run-memory profile-run-syscalls ## Run all profiling tools for selected PROFILE_BUILD

.PHONY: profile-compare-artifacts-output
profile-compare-artifacts-output:
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_TIME_OUTPUT))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_TIME_OUTPUT))
	diff -u "$(PROFILE_COMPARE_A_TIME_OUTPUT)" "$(PROFILE_COMPARE_B_TIME_OUTPUT)"

.PHONY: profile-compare-artifacts-time
profile-compare-artifacts-time:
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_HYPERFINE_JSON))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_HYPERFINE_JSON))
	@jq -n -r --arg a "$(PROFILE_BUILD_A)" --arg b "$(PROFILE_BUILD_B)" --slurpfile before "$(PROFILE_COMPARE_A_HYPERFINE_JSON)" --slurpfile after "$(PROFILE_COMPARE_B_HYPERFINE_JSON)" 'def row($$name; $$x; $$y): "\($$name)\t\($$x)\t\($$y)\t\($$y - $$x)\t\(if $$x == 0 then "n/a" else (($$y - $$x) / $$x * 100) end)"; ($$before[0].results[0]) as $$x | ($$after[0].results[0]) as $$y | "metric\t\($$a)\t\($$b)\tdelta\tdelta_pct", row("mean"; $$x.mean; $$y.mean), row("stddev"; $$x.stddev; $$y.stddev), row("median"; $$x.median; $$y.median), row("min"; $$x.min; $$y.min), row("max"; $$x.max; $$y.max)'

.PHONY: profile-compare-stat
profile-compare-stat: ## Interleaved benchmark and hypothesis test for PROFILE_BUILD_A vs PROFILE_BUILD_B
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_BIN))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_BIN))
	$(call clear_profile_compare_stat_results_if_needed)
	@mkdir -p $(PROFILE_COMPARE_STAT_ROOT)
	$(call profile_command_for,$(PROFILE_COMPARE_A_BIN),$(PROFILE_COMPARE_STAT_A_OUTPUT))
	$(call profile_command_for,$(PROFILE_COMPARE_B_BIN),$(PROFILE_COMPARE_STAT_B_OUTPUT))
	diff -u "$(PROFILE_COMPARE_STAT_A_OUTPUT)" "$(PROFILE_COMPARE_STAT_B_OUTPUT)"
	hyperfine --warmup $(PROFILE_COMPARE_WARMUP) --runs $(PROFILE_COMPARE_RUNS) $(PROFILE_COMPARE_HYPERFINE_ARGS) --export-json $(PROFILE_COMPARE_STAT_JSON) \
		--command-name "$(PROFILE_BUILD_A)" '$(call profile_command_for,$(PROFILE_COMPARE_A_BIN),$(PROFILE_COMPARE_STAT_A_OUTPUT))' \
		--command-name "$(PROFILE_BUILD_B)" '$(call profile_command_for,$(PROFILE_COMPARE_B_BIN),$(PROFILE_COMPARE_STAT_B_OUTPUT))'
	env UV_CACHE_DIR=/tmp/uv-cache uv run --no-project --with scipy --with numpy python3 tools/analyze_hyperfine_compare.py \
		--json "$(PROFILE_COMPARE_STAT_JSON)" \
		--summary-output "$(PROFILE_COMPARE_STAT_ANALYSIS)" \
		--baseline "$(PROFILE_BUILD_A)" \
		--candidate "$(PROFILE_BUILD_B)" \
		--min-speedup "$(PROFILE_COMPARE_MIN_SPEEDUP)" \
		--alpha "$(PROFILE_COMPARE_ALPHA)"

.PHONY: profile-compare-artifacts-cpu
profile-compare-artifacts-cpu:
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_PERF_DATA))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_PERF_DATA))
	perf diff --compute delta --sort comm,dso,symbol "$(PROFILE_COMPARE_A_PERF_DATA)" "$(PROFILE_COMPARE_B_PERF_DATA)"

.PHONY: profile-compare-artifacts-memory
profile-compare-artifacts-memory:
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_HEAPTRACK_DATA))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_HEAPTRACK_DATA))
	@echo "== $(PROFILE_BUILD_A) =="
	heaptrack_print "$(PROFILE_COMPARE_A_HEAPTRACK_DATA)" | sed -n '1,80p'
	@echo "== $(PROFILE_BUILD_B) =="
	heaptrack_print "$(PROFILE_COMPARE_B_HEAPTRACK_DATA)" | sed -n '1,80p'

.PHONY: profile-compare-artifacts-syscalls
profile-compare-artifacts-syscalls:
	$(call require_profile_compare_builds,$@)
	$(call require_profile_compare_file,$(PROFILE_COMPARE_A_STRACE_SUMMARY))
	$(call require_profile_compare_file,$(PROFILE_COMPARE_B_STRACE_SUMMARY))
	diff -u "$(PROFILE_COMPARE_A_STRACE_SUMMARY)" "$(PROFILE_COMPARE_B_STRACE_SUMMARY)" || true

.PHONY: profile-compare-artifacts
profile-compare-artifacts: profile-compare-artifacts-output profile-compare-artifacts-time profile-compare-artifacts-cpu profile-compare-artifacts-memory profile-compare-artifacts-syscalls ## Compare saved profiling artifacts for PROFILE_BUILD_A and PROFILE_BUILD_B

.PHONY: profile-clean
profile-clean: ## Remove profiling outputs
	rm -rf $(PROFILE_RESULTS_ROOT)
