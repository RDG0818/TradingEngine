.PHONY: all build rebuild test bench run clean

all: build

build:
	cmake -B build -S . && cmake --build build -- -j$(shell nproc)

rebuild: clean build

test: build
	./build/tests

bench: build
	cmake --build build --target benchmarks -- -j$(shell nproc)
	./build/benchmarks

run: build
	./build/trading_engine

clean:
	rm -rf build/
