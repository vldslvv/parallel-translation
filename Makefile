BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/Debug
RELEASE_DIR := $(BUILD_DIR)/Release
PREFIX      := $(HOME)/.local

.DEFAULT_GOAL := build
.SUFFIXES:

include $(sort $(wildcard mk/*.mk))

##@ General

.PHONY: help
help: ## Show this help message
	@echo "Usage: make [target] [ARGS=\"...\"]"
	@echo ""
	@awk ' \
		/^##@ / { \
			if (seen_section) print ""; \
			seen_section = 1; \
			printf "%s:\n", substr($$0, 5); \
			next; \
		} \
		/^[A-Za-z0-9_.-]+:.*## / { \
			target = $$1; \
			sub(/:.*/, "", target); \
			desc = $$0; \
			sub(/^[^#]*## /, "", desc); \
			printf "  %-32s %s\n", target, desc; \
		} \
	' $(MAKEFILE_LIST)
	@echo ""
	@echo "Variables:"
	@echo "  ARGS                         Arguments forwarded to the binary when using 'run'"
	@echo "                               Example: make run ARGS=\"--input in.txt --output out.txt\""
	@echo "  PROFILE_INPUT                Input text for profile targets"
	@echo "  PROFILE_BUILD                Named profiling root under build/profile-<name>"
	@echo "                               Example: make profile-run-suite PROFILE_BUILD=baseline"
	@echo "  PROFILE_BUILD_A              First named profiling root for profile-compare targets"
	@echo "  PROFILE_BUILD_B              Second named profiling root for profile-compare targets"
	@echo "                               Example: make profile-compare-artifacts PROFILE_BUILD_A=before PROFILE_BUILD_B=after"
	@echo "                               Example: make profile-compare-stat PROFILE_BUILD_A=baseline PROFILE_BUILD_B=candidate"
	@echo "  PROFILE_COMPARE_RUNS         Benchmark comparison runs (default: 30)"
	@echo "  PROFILE_COMPARE_WARMUP       Benchmark comparison warmup runs (default: 5)"
	@echo "  PROFILE_COMPARE_MIN_SPEEDUP  Required candidate speedup percentage (default: 1)"
	@echo "  PROFILE_COMPARE_ALPHA        Hypothesis test alpha (default: 0.05)"
	@echo "  PROFILE_COMPARE_HYPERFINE_ARGS Extra args for benchmark comparison hyperfine"
	@echo "  PROFILE_PARALLELISM          Backend parallelism for profile targets"
	@echo "  PROFILE_BACKEND              Backend provider for profile targets"
	@echo "  PROFILE_POSTPROCESSOR        Postprocessor provider for profile targets"
	@echo "  PROFILE_LOG_LEVEL            Log level for profile targets"
	@echo ""
	@echo "Binary options:"
	@echo "  -h, --help                   Show binary help and exit"
	@echo "  -v, --version                Show version and exit"
	@echo "  -i, --input FILE             Input file path (required)"
	@echo "  -o, --output FILE            Output file path (required)"
