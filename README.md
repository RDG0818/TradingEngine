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
- Current bug with the live_orders_ map in the tradingSystem.cpp area
  - only adds orders from manual trading, not automated traders
  - this messes with the latency calculations and the active order count on the frontend

- PnL calculations on the frontend

- Live Event Log integration and general logging for events

- Postgres for authentification and logging user stats/state/metrics

- Websockets for the candlestick data on the manual trading page
  - make logic for post requests on the orders

- Frontend/backend integration on automated trader page
  - Need to setup pydantic structs on backend and traderInfo in C++
  - post field info to backend/call C++ create_..._trader method
  - get_all_traders endpoint
  - post running status/add logic in C++ to turn on/off trader
  - add quantity field to create trader menu
  - track Trader's balance/pnl/latency/orders per second

- tick interval logic needs to be connected
  - Possibly switch how the tick interval logic works in the backend

- authentification needs to be added (emails/passwords/sign-in)

- Implement different algorithm traders (random, momentum, etc.)
- Incorporate more of boost
- manual trading through frontend
- add performance to README

## License
This project is licensed under the MIT License - see the `License.md` file for details.