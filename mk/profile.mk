##@ Profiling

PROFILE_BUILD_TYPE := RelWithDebInfo
PROFILE_BUILD ?=
PROFILE_BUILD_SUFFIX := $(if $(PROFILE_BUILD),-$(PROFILE_BUILD),)
PROFILE_BUILD_LABEL := $(if $(PROFILE_BUILD),$(PROFILE_BUILD),<default>)
PROFILE_BUILD_ARG := $(if $(PROFILE_BUILD), PROFILE_BUILD=$(PROFILE_BUILD),)
PROFILE_DIR := $(BUILD_DIR)/Profile$(PROFILE_BUILD_SUFFIX)
PROFILE_RESULTS_DIR := $(BUILD_DIR)/profile$(PROFILE_BUILD_SUFFIX)
PROFILE_GENERATORS_DIR := $(PROFILE_DIR)/build/$(PROFILE_BUILD_TYPE)/generators

PROFILE_INPUT := texts/latin/de_magia.txt
PROFILE_OUTPUT := $(PROFILE_RESULTS_DIR)/output.txt
PROFILE_PARALLELISM := 1
PROFILE_BACKEND := stub
PROFILE_POSTPROCESSOR := morpheus
PROFILE_LOG_LEVEL := off

PROFILE_BIN := $(PROFILE_DIR)/parallel-translation
# Use PROFILE_BUILD=<name> to keep separate profiling builds and results, for
# example build/Profile-cached-paths and build/profile-cached-paths/perf.data.
PERF_DATA := $(PROFILE_RESULTS_DIR)/perf.data
# Do not profile forked helpers such as Morpheus; keep CPU samples scoped to
# the parallel-translation process we are optimizing.
PROFILE_PERF_RECORD_ARGS := --call-graph dwarf --no-inherit
PROFILE_ARGS := \
	--reader-path $(PROFILE_INPUT) \
	--writer-path $(PROFILE_OUTPUT) \
	--backend-provider $(PROFILE_BACKEND) \
	--postprocessor-provider $(PROFILE_POSTPROCESSOR) \
	--backend-parallelism $(PROFILE_PARALLELISM) \
	--log-level $(PROFILE_LOG_LEVEL)

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
	@mkdir -p $(PROFILE_RESULTS_DIR)
	$(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-time
profile-time: profile-build ## Benchmark wall time with hyperfine
	@mkdir -p $(PROFILE_RESULTS_DIR)
	hyperfine --warmup 2 --runs 10 '$(PROFILE_BIN) $(PROFILE_ARGS)'

.PHONY: profile-cpu
profile-cpu: profile-build ## Record CPU profile with perf
	@if [ -f "$(PERF_DATA)" ]; then \
		printf '%s already exists. Clear profiling data for PROFILE_BUILD=%s? [y/N] ' "$(PERF_DATA)" "$(PROFILE_BUILD_LABEL)"; \
		read answer; \
		case "$$answer" in \
			y|Y) rm -rf "$(PROFILE_RESULTS_DIR)" ;; \
			*) echo "aborted; run 'make profile-clean$(PROFILE_BUILD_ARG)' to clear it."; exit 1 ;; \
		esac; \
	fi
	@mkdir -p $(PROFILE_RESULTS_DIR)
	perf record $(PROFILE_PERF_RECORD_ARGS) --output $(PERF_DATA) -- $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-cpu-report
profile-cpu-report: ## Open perf report for saved profile
	perf report --input $(PERF_DATA)

.PHONY: profile-memory
profile-memory: profile-build ## Profile allocations with heaptrack
	@mkdir -p $(PROFILE_RESULTS_DIR)
	heaptrack $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-syscalls
profile-syscalls: profile-build ## Summarize syscalls and subprocesses with strace
	@mkdir -p $(PROFILE_RESULTS_DIR)
	strace -f -c $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-compare-postprocessor
profile-compare-postprocessor: profile-build ## Compare Morpheus postprocessing against none
	@mkdir -p $(PROFILE_RESULTS_DIR)
	hyperfine --warmup 2 --runs 10 \
		'$(PROFILE_BIN) $(PROFILE_ARGS)' \
		'$(PROFILE_BIN) --reader-path $(PROFILE_INPUT) --writer-path $(PROFILE_RESULTS_DIR)/output-none.txt --backend-provider $(PROFILE_BACKEND) --postprocessor-provider none --backend-parallelism $(PROFILE_PARALLELISM) --log-level $(PROFILE_LOG_LEVEL)'

.PHONY: profile-clean
profile-clean: ## Remove profiling outputs
	rm -rf $(PROFILE_RESULTS_DIR)
