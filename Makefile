BUILD_DIR     := build

DEBUG_DIR     := $(BUILD_DIR)/Debug
RELEASE_DIR   := $(BUILD_DIR)/Release
PREFIX        := $(HOME)/.local
CONAN_PROFILE := $(HOME)/.conan2/profiles/default
MORPHEUS_RECIPE := conan/recipes/morpheus

$(CONAN_PROFILE):
	conan profile detect

.PHONY: morpheus-recipe
morpheus-recipe:
	conan export $(MORPHEUS_RECIPE)

.PHONY: all
all: build

.PHONY: build
build: $(CONAN_PROFILE) morpheus-recipe
	conan install . --build=missing -s build_type=Debug
	cmake -B $(DEBUG_DIR) -DCMAKE_TOOLCHAIN_FILE=$(DEBUG_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(DEBUG_DIR)

.PHONY: release
release: $(CONAN_PROFILE) morpheus-recipe
	conan install . --build=missing -s build_type=Release
	cmake -B $(RELEASE_DIR) -DCMAKE_TOOLCHAIN_FILE=$(RELEASE_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

.PHONY: install
install: release
	cmake --install $(RELEASE_DIR) --prefix $(PREFIX)

.PHONY: uninstall
uninstall:
	xargs rm -f < $(RELEASE_DIR)/install_manifest.txt

.PHONY: format
format:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

.PHONY: tidy
tidy:
	run-clang-tidy -p $(DEBUG_DIR) src/ tests/

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: run
run: release
	./$(RELEASE_DIR)/parallel-translation $(ARGS)

.PHONY: run-debug
run-debug: build
	./$(DEBUG_DIR)/parallel-translation $(ARGS)

.PHONY: test
test: build
	ctest --test-dir $(DEBUG_DIR) --output-on-failure

.PHONY: help
help:
	@echo "Usage: make [target] [ARGS=\"...\"]"
	@echo ""
	@echo "Targets:"
	@echo "  build          Debug build (default)"
	@echo "  release        Optimized release build"
	@echo "  run            Build and run the binary (release)"
	@echo "  run-debug      Build and run the binary (debug)"
	@echo "  test           Build and run tests"
	@echo "  install        Build (release) and install to system (may need sudo)"
	@echo "  uninstall      Remove installed files"
	@echo "  format         Auto-format source files with clang-format"
	@echo "  tidy           Run clang-tidy static analysis on src/ and tests/"
	@echo "  clean          Remove the build directory"
	@echo "  help           Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  ARGS           Arguments forwarded to the binary when using 'run'"
	@echo "                 Example: make run ARGS=\"--input in.txt --output out.txt\""
	@echo ""
	@echo "Binary options:"
	@echo "  -h, --help           Show binary help and exit"
	@echo "  -v, --version        Show version and exit"
	@echo "  -i, --input FILE     Input file path (required)"
	@echo "  -o, --output FILE    Output file path (required)"
