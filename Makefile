all: build 

build:
	@echo "Building C++ components..."
	@rm -rf build
	@cmake -B build -S . -DPython3_EXECUTABLE=$(which python3)
	@cmake --build build
	@cd build
	@python3 -m pybind11_stubgen -m trading_engine_py --output-dir=.
	@touch __init__.py
	@cd .. 
	@echo "Build complete."

stubs: build
	@echo "Generating Python stubs..."
	@PYTHONPATH=./python stubgen -m trading_engine.trading_core -o python/trading_engine
	@echo "Stub generation complete."

test: build
	@echo "Running C++ tests..."
	@./build/my_tests
	@echo "Running Python tests..."
	@PYTHONPATH=./python pytest
	@echo "All tests completed."

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build
	@echo "Clean complete."