##@ Build

.PHONY: all
all: build ## Debug build (default)

.PHONY: build
build: .conan-profile .morpheus-recipe ## Build the debug binary
	$(CONAN) install . --build=missing -s build_type=Debug -s compiler.cppstd=$(CONAN_CPPSTD)
	cmake -B $(DEBUG_DIR) -DCMAKE_TOOLCHAIN_FILE=$(DEBUG_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(DEBUG_DIR)

.PHONY: release
release: .conan-profile .morpheus-recipe ## Build the optimized release binary
	$(CONAN) install . --build=missing -s build_type=Release -s compiler.cppstd=$(CONAN_CPPSTD)
	cmake -B $(RELEASE_DIR) -DCMAKE_TOOLCHAIN_FILE=$(RELEASE_DIR)/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

.PHONY: install
install: release ## Build release and install to PREFIX
	cmake --install $(RELEASE_DIR) --prefix $(PREFIX)

.PHONY: uninstall
uninstall: ## Remove installed files from the release install manifest
	xargs rm -f < $(RELEASE_DIR)/install_manifest.txt

.PHONY: clean
clean: ## Remove the build directory
	rm -rf $(BUILD_DIR)

##@ Run And Test

.PHONY: run
run: release ## Build and run the release binary
	./$(RELEASE_DIR)/parallel-translation $(ARGS)

.PHONY: run-debug
run-debug: build ## Build and run the debug binary
	./$(DEBUG_DIR)/parallel-translation $(ARGS)

.PHONY: test
test: build ## Build and run tests
	ctest --test-dir $(DEBUG_DIR) --output-on-failure
