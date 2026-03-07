.PHONY: all build release clean run test help

BUILD_DIR := build

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

release:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/parallel-translation $(ARGS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

help:
	@echo "Usage: make [target] [ARGS=\"...\"]"
	@echo ""
	@echo "Targets:"
	@echo "  build          Debug build (default)"
	@echo "  release        Optimized release build"
	@echo "  run            Build and run the binary"
	@echo "  test           Build and run tests"
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
