#pragma once
#include "matchingEngine.h"
#include "eventDispatcher.h"
#include <iostream>
#include <random>
#include <vector>

enum class TraderType : std::uint8_t {
    RANDOM,
    MARKET_MAKER,
    MOMENTUM,
    MEAN_REVERSION,
    LIQUIDITY
};

// Abstract class for other Trader types

class Trader {
private:

    TraderType type_;
    TraderID traderID_;
    EventDispatcher& dispatcher_;
    MatchingEngine& engine_;

protected:
    std::vector<std::string> symbols_;

    MatchingEngine& engine() { return engine_; }
    EventDispatcher& dispatcher() { return dispatcher_; }

public:

    Trader(TraderType type, MatchingEngine& engine, EventDispatcher& dispatcher, TraderID traderID, const std::vector<std::string>& symbols) 
        : type_(type), engine_(engine), dispatcher_(dispatcher), traderID_(traderID), symbols_(symbols) {};
    virtual void tick() = 0;
    TraderType getType() {return type_;}
    TraderID getID() {return traderID_;}
};

// class to run all Traders tick function on a single thread

class TraderManager {
private:

    std::vector<std::unique_ptr<Trader>> traders;
    std::chrono::milliseconds tick_interval;
    std::atomic<bool> running{false};
    std::thread thread;

    void run() {
        while (running) {
            for (auto& trader : traders) {
                trader->tick();
            }
            std::this_thread::sleep_for(tick_interval);
        }
    };

public:
    TraderManager(std::chrono::milliseconds tick_interval) : tick_interval(tick_interval) {};
    ~TraderManager() {stop();};

    void addTrader(std::unique_ptr<Trader> trader) {
        traders.push_back(std::move(trader));
    };

    void start() {
        running = true;
        thread = std::thread(&TraderManager::run, this);
    };

    void stop() {
        running = false;
        if (thread.joinable()) {
            thread.join();
        }
    };

};

// Lambda is average per second

class LiquidityTrader : public Trader {
private:
    std::random_device rd;
    std::mt19937 gen;
    // Distribution is initialized with lambda in orders/sec, so it produces values in seconds.
    std::exponential_distribution<> exp_dist;
    
    // Internal timekeeping is now done in seconds using floating-point for precision.
    double time_delta_s;
    double time_until_order_s;

    std::uniform_int_distribution<int> side_dist;
    std::uniform_int_distribution<int> quantity_dist;
    std::uniform_int_distribution<int> symbol_dist;

public:

    // The 'lambda' parameter is interpreted as average orders per second.
    LiquidityTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID traderID, 
        float lambda, std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity)
    : Trader(TraderType::LIQUIDITY, engine, dispatcher, traderID, symbols),
      gen(rd()),
      exp_dist(lambda), // Use lambda (orders/sec) directly
      time_delta_s(std::chrono::duration<double>(time_delta).count()), // Convert tick interval to seconds
      side_dist(0, 1),
      quantity_dist(1, max_quantity),
      symbol_dist(0, symbols.empty() ? 0 : symbols.size() - 1)
    {
        // exp_dist(gen) returns time in seconds.
        time_until_order_s = exp_dist(gen);
    }

    void tick() override {
        static int tick_count = 0;
        tick_count++;

        if (tick_count <= 20) { // Print for first 20 ticks to avoid spam
            std::cout << "[Tick " << tick_count << "] time_until_order_s: " << time_until_order_s << ", time_delta_s: " << time_delta_s << std::endl;
        }

        if (symbols_.empty()) {
            return;
        }

        time_until_order_s -= time_delta_s;

        if (time_until_order_s <= 0) {
            if (tick_count <= 20) {
                std::cout << "[Tick " << tick_count << " ] Submitting order!" << std::endl;
            }
            RawOrderParams rop = {
                .symbol = symbols_[symbol_dist(gen)],
                .orderType = OrderType::MARKET,
                .side = side_dist(gen) ? Side::BUY : Side::SELL,
                .price = "",
                .stopPrice = "",
                .quantity = static_cast<Quantity>(quantity_dist(gen)),
                .traderID = getID(),
            };

            engine().submitOrder(rop);
            time_until_order_s = exp_dist(gen);
            if (tick_count <= 20) {
                std::cout << "[Tick " << tick_count << " ] Reset time_until_order_s to: " << time_until_order_s << std::endl;
            }
        }

        }
    };
