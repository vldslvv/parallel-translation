##@ Profiling

PROFILE_BUILD_TYPE := RelWithDebInfo
PROFILE_BUILD ?=
PROFILE_BUILD_SUFFIX := $(if $(PROFILE_BUILD),-$(PROFILE_BUILD),)
PROFILE_BUILD_LABEL := $(if $(PROFILE_BUILD),$(PROFILE_BUILD),<default>)
PROFILE_BUILD_ARG := $(if $(PROFILE_BUILD), PROFILE_BUILD=$(PROFILE_BUILD),)
PROFILE_DIR := $(BUILD_DIR)/Profile$(PROFILE_BUILD_SUFFIX)
PROFILE_RESULTS_ROOT := $(BUILD_DIR)/profile$(PROFILE_BUILD_SUFFIX)
PROFILE_RUN_DIR := $(PROFILE_RESULTS_ROOT)/run
PROFILE_TIME_DIR := $(PROFILE_RESULTS_ROOT)/time
PROFILE_PERF_DIR := $(PROFILE_RESULTS_ROOT)/perf
PROFILE_MEMORY_DIR := $(PROFILE_RESULTS_ROOT)/memory
PROFILE_SYSCALLS_DIR := $(PROFILE_RESULTS_ROOT)/syscalls
PROFILE_COMPARE_DIR := $(PROFILE_RESULTS_ROOT)/compare-postprocessor
PROFILE_GENERATORS_DIR := $(PROFILE_DIR)/build/$(PROFILE_BUILD_TYPE)/generators

PROFILE_INPUT := texts/latin/de_magia.txt
PROFILE_RUN_OUTPUT := $(PROFILE_RUN_DIR)/output.txt
PROFILE_TIME_OUTPUT := $(PROFILE_TIME_DIR)/output.txt
PROFILE_PERF_OUTPUT := $(PROFILE_PERF_DIR)/output.txt
PROFILE_MEMORY_OUTPUT := $(PROFILE_MEMORY_DIR)/output.txt
PROFILE_SYSCALLS_OUTPUT := $(PROFILE_SYSCALLS_DIR)/output.txt
PROFILE_COMPARE_OUTPUT := $(PROFILE_COMPARE_DIR)/output.txt
PROFILE_COMPARE_NONE_OUTPUT := $(PROFILE_COMPARE_DIR)/output-none.txt
PROFILE_PARALLELISM := 1
PROFILE_BACKEND := stub
PROFILE_POSTPROCESSOR := morpheus
PROFILE_LOG_LEVEL := off

PROFILE_BIN := $(PROFILE_DIR)/parallel-translation
# Use PROFILE_BUILD=<name> to keep separate profiling builds and results, for
# example build/Profile-cached-paths and build/profile-cached-paths/perf/perf.data.
PERF_DATA := $(PROFILE_PERF_DIR)/perf.data
HYPERFINE_JSON := $(PROFILE_TIME_DIR)/hyperfine.json
HEAPTRACK_DATA := $(PROFILE_MEMORY_DIR)/heaptrack.data
STRACE_SUMMARY := $(PROFILE_SYSCALLS_DIR)/strace-summary.txt
PROFILE_COMPARE_JSON := $(PROFILE_COMPARE_DIR)/compare-postprocessor.json
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

.PHONY: profile-build
profile-build: conan-profile morpheus-recipe ## Build optimized binary with debug symbols
	$(CONAN) install . --build=missing -of $(PROFILE_DIR) -s build_type=$(PROFILE_BUILD_TYPE) -s compiler.cppstd=$(CONAN_CPPSTD)
	cmake -B $(PROFILE_DIR) \
		-DCMAKE_TOOLCHAIN_FILE=$(PROFILE_GENERATORS_DIR)/conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=$(PROFILE_BUILD_TYPE) \
		-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g -fno-omit-frame-pointer"
	cmake --build $(PROFILE_DIR)

.PHONY: profile-run
profile-run: profile-build ## Run the default profiling workload once
	$(call clear_profile_results_if_needed,$(PROFILE_RUN_DIR))
	@mkdir -p $(PROFILE_RUN_DIR)
	$(call profile_command,$(PROFILE_RUN_OUTPUT))

.PHONY: profile-time
profile-time: profile-build ## Benchmark wall time with hyperfine
	$(call clear_profile_results_if_needed,$(PROFILE_TIME_DIR))
	@mkdir -p $(PROFILE_TIME_DIR)
	hyperfine --warmup 2 --runs 10 --export-json $(HYPERFINE_JSON) '$(call profile_command,$(PROFILE_TIME_OUTPUT))'

.PHONY: profile-cpu
profile-cpu: profile-build ## Record CPU profile with perf
	$(call clear_profile_results_if_needed,$(PROFILE_PERF_DIR))
	@mkdir -p $(PROFILE_PERF_DIR)
	perf record $(PROFILE_PERF_RECORD_ARGS) --output $(PERF_DATA) -- $(call profile_command,$(PROFILE_PERF_OUTPUT))

.PHONY: profile-cpu-report
profile-cpu-report: ## Open perf report for saved profile
	perf report --input $(PERF_DATA)

.PHONY: profile-memory
profile-memory: profile-build ## Profile allocations with heaptrack
	$(call clear_profile_results_if_needed,$(PROFILE_MEMORY_DIR))
	@mkdir -p $(PROFILE_MEMORY_DIR)
	heaptrack --output $(HEAPTRACK_DATA) $(call profile_command,$(PROFILE_MEMORY_OUTPUT))

.PHONY: profile-syscalls
profile-syscalls: profile-build ## Summarize syscalls and subprocesses with strace
	$(call clear_profile_results_if_needed,$(PROFILE_SYSCALLS_DIR))
	@mkdir -p $(PROFILE_SYSCALLS_DIR)
	strace -f -c -o $(STRACE_SUMMARY) $(call profile_command,$(PROFILE_SYSCALLS_OUTPUT))

.PHONY: profile-compare-postprocessor
profile-compare-postprocessor: profile-build ## Compare Morpheus postprocessing against none
	$(call clear_profile_results_if_needed,$(PROFILE_COMPARE_DIR))
	@mkdir -p $(PROFILE_COMPARE_DIR)
	hyperfine --warmup 2 --runs 10 --export-json $(PROFILE_COMPARE_JSON) \
		'$(call profile_command,$(PROFILE_COMPARE_OUTPUT))' \
		'$(PROFILE_BIN) --reader-path $(PROFILE_INPUT) --writer-path $(PROFILE_COMPARE_NONE_OUTPUT) --backend-provider $(PROFILE_BACKEND) --postprocessor-provider none --backend-parallelism $(PROFILE_PARALLELISM) --log-level $(PROFILE_LOG_LEVEL)'

.PHONY: profile-all
profile-all: profile-run profile-time profile-cpu profile-memory profile-syscalls ## Run all profiling tools for selected PROFILE_BUILD

.PHONY: profile-clean
profile-clean: ## Remove profiling outputs
	rm -rf $(PROFILE_RESULTS_ROOT)
