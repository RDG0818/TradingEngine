# Talat 

Talat is a high-performance simulated trading exchange built in C++, Python, and React.  

Talat means 'market' in Thai.

## Overview

## Features
- Matching Engine
- Orderbook
- React Frontend
- FastAPI Backend
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
- Automated Traders for Realistic Market Movements (Poisson Distribution)
    - Market Maker Bot (Following Geometric Brownian Motion)
    - Random Limit Order Bot
    - Random Market Order Bot
- Google Test
- Google Benchmark 

## Getting Started

### Prerequisites
- A C++ compiler that supports C++20
- CMake (version 3.12+)
- Python 3.10
- Conda

### Installation
```bash
git clone https://github.com/RDG0818/TradingEngine.git
cd TradingEngine
git submodule --init --recursive
conda create -n talat python=3.10
conda activate talat
cd backend
pip install -r requirements.txt
cd ..
make
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
- Incorporate more of boost
- manual trading through frontend
- add performance to README

- Integration with a visualization library for plotting equity curves and performance metrics
- Support for options and other derivatives
- Support for live paper trading on crypto
- Optimizations for faster high frequency trading
- examples of portfolio optimization and advanced trading strategies

## License
This project is licensed under the MIT License - see the `License.md` file for details.