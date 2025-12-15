# High-Frequency Trading Engine  

A high-performance trading engine built in C++.

## Overview

## Features
- Matching Engine
- Orderbook
- Support for the following order types:
    - Limit
    - Stop Limit
    - Market
    - Stop Market
- Support for the following Time in Force:
    - Good 'Til Canceled (GTC)
    - Fill or Kill (FOK)
    - Immediate or Cancel (IOC)
- Level 1/2/3 Feed Information
- Automated Traders for Realistic Market Movements
    - Market Maker Bot
    - Random Limit Order Bot
    - Random Market Order Bot
- Google Test
- Google Benchmark 

## Getting Started

### Prerequisites
- A C++ compiler that supports C++20
- CMake (version 3.12+)

### Installation
```bash
git clone https://github.com/RDG0818/TradingEngine.git
cd TradingEngine

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
- Implement different algorithm traders (random, momentum, etc.)

- Integration with a visualization library for plotting equity curves and performance metrics
- Support for options and other derivatives
- Support for live paper trading on crypto
- Optimizations for faster high frequency trading
- examples of portfolio optimization and advanced trading strategies

## License
This project is licensed under the MIT License - see the `License.md` file for details.