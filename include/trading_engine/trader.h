#pragma once
#include "matchingEngine.h"
#include "eventDispatcher.h"

enum class TraderType : std::uint8_t {
    RANDOM,
    MARKET_MAKER,
    MOMENTUM,
    MEAN_REVERSION,
    LIQUIDITY
};

class Trader {
private:

    TraderType type;
    EventDispatcher& dispatcher;
    MatchingEngine& engine;

public:

    Trader(TraderType type, MatchingEngine engine, EventDispatcher dispatcher) : type(type), engine(engine), dispatcher(dispatcher) {};
    virtual void tick() = 0;
};

class TraderManager {
private:
void run();

std::vector<std::unique_ptr<Trader>> traders;
std::chrono::milliseconds tick_interval;
std::atomic<bool> running{false};
std::thread thread;


public:
    TraderManager(std::chrono::milliseconds tick_interval) : tick_interval(tick_interval) {};
    ~TraderManager();

    void addTrader(std::unique_ptr<Trader> trader);
    void start();
    void stop();

};