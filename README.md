# High-Frequency Trading Engine  

A high-performance trading engine built in C++.

## Overview

## Features
- Matching Engine
- Orderbook
- Support for the following order types:
    - Limit and Stop Limit
    - Market and Stop Market
- Support for the following Time in Force:
    - Good 'Til Canceled (GTC)
    - Fill or Kill (FOK)
    - Immediate or Cancel (IOC)
- Google Test
- Google Benchmark (Include results)

## Getting Started

### Prerequisites
- A C++ compiler that supports C++20
- CMake (version 3.12+)

### Installation
```bash
git clone https://github.com/RDG0818/TradingEngine.git
cd TradingEngine
git submodule update --init --recursive
```

**Build the C++ core:**
```bash
cmake -B build -S .
cmake --build build
```

**(Optional) Running Unit Tests:**
```bash
./build/my_tests
./build/my_benchmarks
```
## Usage

## Future Work

TODO:
- Implement orderbook dissemination logic
- Implement different algorithm traders (random, momentum, etc.)
- Implement risk layer

- Support for multiple symbols in a single backtest
- Implementation of more complex order types (e.g. Stop-Loss, Iceberg)
- Integration with a visualization library for plotting equity curves and performance metrics
- Support for options and other derivatives
- Support for live paper trading on crypto
- Optimizations for faster high frequency trading
- examples of portfolio optimization and advanced trading strategies

## License
This project is licensed under the MIT License - see the `License.md` file for details.