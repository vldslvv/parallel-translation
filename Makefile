.PHONY: all build release clean run test format install uninstall help

BUILD_DIR     := build

# PoDoFo 0.10.x creates its own OpenSSL library context (OSSL_LIB_CTX_new)
# which does not inherit system-wide openssl.cnf provider settings.
# OPENSSL_MODULES tells the custom context where to find provider modules
# (e.g. legacy.so for RC4 support required by PDF encryption).
export OPENSSL_MODULES := /usr/lib64/ossl-modules
DEBUG_DIR     := $(BUILD_DIR)/Debug
RELEASE_DIR   := $(BUILD_DIR)/Release
PREFIX        := $(HOME)/.local
CONAN_PROFILE := $(HOME)/.conan2/profiles/default

$(CONAN_PROFILE):
	conan profile detect

all: build

build: $(CONAN_PROFILE)
	conan install . --build=missing -s build_type=Debug
	cmake -B $(DEBUG_DIR) -DCMAKE_TOOLCHAIN_FILE=$(DEBUG_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(DEBUG_DIR)

release: $(CONAN_PROFILE)
	conan install . --build=missing -s build_type=Release
	cmake -B $(RELEASE_DIR) -DCMAKE_TOOLCHAIN_FILE=$(RELEASE_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

install: release
	cmake --install $(RELEASE_DIR) --prefix $(PREFIX)

uninstall:
	xargs rm -f < $(RELEASE_DIR)/install_manifest.txt

format:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

clean:
	rm -rf $(BUILD_DIR)

run: build
	./$(DEBUG_DIR)/parallel-translation $(ARGS)

test: build
	ctest --test-dir $(DEBUG_DIR) --output-on-failure

help:
	@echo "Usage: make [target] [ARGS=\"...\"]"
	@echo ""
	@echo "Targets:"
	@echo "  build          Debug build (default)"
	@echo "  release        Optimized release build"
	@echo "  run            Build and run the binary"
	@echo "  test           Build and run tests"
	@echo "  install        Build (release) and install to system (may need sudo)"
	@echo "  uninstall      Remove installed files"
	@echo "  format         Auto-format source files with clang-format"
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
