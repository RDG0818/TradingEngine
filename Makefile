.PHONY: all build test cpp-test bench clean rebuild

PYTHON := $(shell conda run -n talat which python3 2>/dev/null || which python3)
BUILD_DIR := build

all: build

build:
	@echo "Configuring..."
	@cmake -B $(BUILD_DIR) -S . -DPython3_EXECUTABLE=$(PYTHON) -DCMAKE_BUILD_TYPE=Release
	@echo "Building..."
	@cmake --build $(BUILD_DIR)
	@echo "Build complete. .so → backend/"

rebuild:
	@rm -rf $(BUILD_DIR)
	@$(MAKE) build

test: build cpp-test

cpp-test:
	@echo "Running C++ tests..."
	@./$(BUILD_DIR)/tests
	@echo "All C++ tests passed."

bench:
	@echo "Building benchmarks..."
	@cmake --build $(BUILD_DIR) --target benchmarks
	@echo "Running benchmarks..."
	@./$(BUILD_DIR)/benchmarks

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned."