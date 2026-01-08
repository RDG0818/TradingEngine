all: build 

build:
	@echo "Building C++ components..."
	@rm -rf build
	@cmake -B build -S . -DPython3_EXECUTABLE=$(which python3)
	@cmake --build build
	@PYTHONPATH=./backend python3 -m pybind11_stubgen trading_engine_py --output-dir=backend --enum-class-locations TimeInForce:trading_engine_py.TimeInForce
	@echo "Build complete."

test: build
	@echo "Running C++ tests..."
	@./build/tests
	@echo "Running Python tests..."
	@PYTHONPATH=./python pytest
	@echo "All tests completed."

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build
	@echo "Clean complete."