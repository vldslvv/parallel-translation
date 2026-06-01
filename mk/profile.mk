##@ Profiling

PROFILE_DIR := $(BUILD_DIR)/Profile
PROFILE_RESULTS_DIR := $(BUILD_DIR)/profile
PROFILE_BUILD_TYPE := RelWithDebInfo
PROFILE_GENERATORS_DIR := $(PROFILE_DIR)/build/$(PROFILE_BUILD_TYPE)/generators

PROFILE_INPUT := texts/latin/de_magia.txt
PROFILE_OUTPUT := $(PROFILE_RESULTS_DIR)/output.txt
PROFILE_PARALLELISM := 1
PROFILE_BACKEND := stub
PROFILE_POSTPROCESSOR := morpheus
PROFILE_LOG_LEVEL := off

PROFILE_BIN := $(PROFILE_DIR)/parallel-translation
PERF_DATA := $(PROFILE_RESULTS_DIR)/perf.data
PROFILE_ARGS := \
	--reader-path $(PROFILE_INPUT) \
	--writer-path $(PROFILE_OUTPUT) \
	--backend-provider $(PROFILE_BACKEND) \
	--postprocessor-provider $(PROFILE_POSTPROCESSOR) \
	--backend-parallelism $(PROFILE_PARALLELISM) \
	--log-level $(PROFILE_LOG_LEVEL)

define require_tool
	@command -v $(1) >/dev/null 2>&1 || { \
		echo "error: $(1) is required for this target."; \
		echo "install hint: sudo apt install $(2)"; \
		exit 1; \
	}
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
	@mkdir -p $(PROFILE_RESULTS_DIR)
	$(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-time
profile-time: profile-build ## Benchmark wall time with hyperfine
	$(call require_tool,hyperfine,hyperfine)
	@mkdir -p $(PROFILE_RESULTS_DIR)
	hyperfine --warmup 2 --runs 10 '$(PROFILE_BIN) $(PROFILE_ARGS)'

.PHONY: profile-cpu
profile-cpu: profile-build ## Record CPU profile with perf
	$(call require_tool,perf,linux-tools-common linux-tools-generic)
	@mkdir -p $(PROFILE_RESULTS_DIR)
	perf record --call-graph dwarf --output $(PERF_DATA) -- $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-cpu-report
profile-cpu-report: ## Open perf report for saved profile
	$(call require_tool,perf,linux-tools-common linux-tools-generic)
	perf report --input $(PERF_DATA)

.PHONY: profile-memory
profile-memory: profile-build ## Profile allocations with heaptrack
	$(call require_tool,heaptrack,heaptrack)
	@mkdir -p $(PROFILE_RESULTS_DIR)
	heaptrack $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-syscalls
profile-syscalls: profile-build ## Summarize syscalls and subprocesses with strace
	$(call require_tool,strace,strace)
	@mkdir -p $(PROFILE_RESULTS_DIR)
	strace -f -c $(PROFILE_BIN) $(PROFILE_ARGS)

.PHONY: profile-compare-postprocessor
profile-compare-postprocessor: profile-build ## Compare Morpheus postprocessing against none
	$(call require_tool,hyperfine,hyperfine)
	@mkdir -p $(PROFILE_RESULTS_DIR)
	hyperfine --warmup 2 --runs 10 \
		'$(PROFILE_BIN) $(PROFILE_ARGS)' \
		'$(PROFILE_BIN) --reader-path $(PROFILE_INPUT) --writer-path $(PROFILE_RESULTS_DIR)/output-none.txt --backend-provider $(PROFILE_BACKEND) --postprocessor-provider none --backend-parallelism $(PROFILE_PARALLELISM) --log-level $(PROFILE_LOG_LEVEL)'

.PHONY: profile-clean
profile-clean: ## Remove profiling outputs
	rm -rf $(PROFILE_RESULTS_DIR)
