#pragma once
#include "trader.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

class TraderManager {
public:
    TraderManager(std::chrono::milliseconds tick_interval);
    ~TraderManager();

    void addTrader(std::unique_ptr<Trader> trader);
    void start();
    void stop();

private:
    void run();

    std::vector<std::unique_ptr<Trader>> traders_;
    std::chrono::milliseconds tick_interval_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};
