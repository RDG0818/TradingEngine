#include "trading_engine/traderManager.h"

TraderManager::TraderManager(std::chrono::milliseconds tick_interval)
    : tick_interval_(tick_interval) {}

TraderManager::~TraderManager() {
    stop();
}

void TraderManager::addTrader(std::unique_ptr<Trader> trader) {
    traders_.push_back(std::move(trader));
}

void TraderManager::start() {
    running_ = true;
    thread_ = std::thread(&TraderManager::run, this);
}

void TraderManager::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void TraderManager::run() {
    while (running_) {
        for (auto& trader : traders_) {
            trader->tick();
        }
        std::this_thread::sleep_for(tick_interval_);
    }
}
