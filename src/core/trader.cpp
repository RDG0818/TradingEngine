#include "trading_engine/trader.h"

TraderManager::~TraderManager() {
    stop();
}

void TraderManager::addTrader(std::unique_ptr<Trader> trader) {
    traders.push_back(std::move(trader));
}

void TraderManager::start() {
    running = true;
    thread = std::thread(&TraderManager::run, this);
}

void TraderManager::stop() { 
    running = false;
    if (thread.joinable()) {
        thread.join();
    }
}

void TraderManager::run() {
    while (running) {
        for (auto& trader : traders) {
            trader->tick();
        }
        std::this_thread::sleep_for(tick_interval);
    }
}