#pragma once
#include "matchingEngine.h"
#include "eventDispatcher.h"
#include "events.h"
#include <iostream>
#include <random>
#include <vector>
#include <cmath>
#include <unordered_map>

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
    std::exponential_distribution<> exp_dist;
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
      exp_dist{lambda}, // Use lambda (orders/sec) directly
      time_delta_s(std::chrono::duration<double>(time_delta).count()), // Convert tick interval to seconds
      side_dist(0, 1),
      quantity_dist(1, max_quantity),
      symbol_dist(0, symbols.empty() ? 0 : symbols.size() - 1)
    {
        time_until_order_s = exp_dist(gen);
    }

    void tick() override {

        if (symbols_.empty()) {
            return;
        }

        time_until_order_s -= time_delta_s;

        while (time_until_order_s <= 0) {
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
            time_until_order_s += exp_dist(gen);

        }

        }
    };

class RandomTrader : public Trader {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::exponential_distribution<> exp_dist;
    double time_delta_s;
    double time_until_order_s;
    double normal_dist_var;

    std::uniform_int_distribution<int> side_dist;
    std::uniform_int_distribution<int> quantity_dist;
    std::uniform_int_distribution<int> symbol_dist;
    std::uniform_int_distribution<int> price_dist;
    std::normal_distribution<double> norm_dist;
public:

    RandomTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID traderID, float lambda, 
        std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity, float norm_dist_var)
        : Trader(TraderType::RANDOM, engine, dispatcher, traderID, symbols),
        gen(rd()),
        exp_dist{lambda},
        time_delta_s(std::chrono::duration<double>(time_delta).count()),
        side_dist(0, 1),
        quantity_dist(1, max_quantity),
        symbol_dist(0, symbols.empty() ? 0 : symbols.size() - 1),
        price_dist(1, 100), // Hardcoded to allow at most a 1 dollar price difference for bid-ask spread if orderbook is empty
        norm_dist(0, norm_dist_var)
        {
            time_until_order_s = exp_dist(gen);
        }
    
    void tick() override {
        if (symbols_.empty()) {
            return;
        }

        time_until_order_s -= time_delta_s;

        while (time_until_order_s <= 0) {
            std::string sym = symbols_[symbol_dist(gen)];
            SymbolID sym_id = SymbolRegistry::getInstance().getID(sym);
            Side side = side_dist(gen) ? Side::BUY : Side::SELL;    
            std::string price;

            std::optional<MarketData> bestBid = engine().getBestBid(sym_id);
            std::optional<MarketData> bestAsk = engine().getBestAsk(sym_id);

            if (!bestBid.has_value() && !bestAsk.has_value()) {
                price = "100.00";
            }
            else {
                if (side == Side::BUY) {
                    if (!bestBid.has_value()) {
                        price = std::to_string(std::max(1UL, bestAsk.value().price - price_dist(gen)));
                    }
                    else {
                        price = std::to_string(std::max(1UL, static_cast<Price>(
                            std::round(bestBid.value().price + norm_dist(gen)*10000))));
                        price.insert(price.end() - 4, '.');
                    }
                }
                else {
                    if (!bestAsk.has_value()) {
                        price = std::to_string(std::max(1UL, bestBid.value().price + price_dist(gen)));
                    }
                    else {
                        price = std::to_string(std::max(1UL, static_cast<Price>(
                            std::round(bestAsk.value().price + norm_dist(gen)*10000))));
                        price.insert(price.end() - 4, '.');
                    }
                }
            }
            
            RawOrderParams rop = {
                .symbol = sym,
                .orderType = OrderType::LIMIT,
                .side = side,
                .price = price,
                .stopPrice = "",
                .quantity = static_cast<Quantity>(quantity_dist(gen)),
                .traderID = getID(),
            };

            engine().submitOrder(rop);
            time_until_order_s += exp_dist(gen);
        }
    }
};

class MarketMakerTrader : public Trader {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::normal_distribution<double> norm_dist;
    std::uniform_int_distribution<int> quantity_dist;

    double mu_;
    double sigma_;
    double spread_;
    double dt_;

    std::unordered_map<std::string, double> fair_prices_;

    // Helper to convert double price to string format
    std::string format_price(double price) {
        if (price <= 0) return "";
        auto price_int = static_cast<Price>(std::round(price * 10000));
        if (price_int <= 0) return "";
        std::string price_str = std::to_string(price_int);
        if (price_str.length() <= 4) {
            price_str.insert(0, 4 - price_str.length() + 1, '0');
        }
        price_str.insert(price_str.length() - 4, ".");
        if (price_str.front() == '.') {
            price_str.insert(0, 1, '0');
        }
        return price_str;
    }

public:
    MarketMakerTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID traderID, 
        const std::vector<std::string>& symbols, double mu, double sigma, double spread,
        std::chrono::milliseconds time_delta, int max_quantity, double initial_price)
        : Trader(TraderType::MARKET_MAKER, engine, dispatcher, traderID, symbols),
        gen(rd()),
        norm_dist(0.0, 1.0),
        quantity_dist(1, max_quantity),
        mu_(mu),
        sigma_(sigma),
        spread_(spread),
        dt_(std::chrono::duration<double>(time_delta).count())
    {
        for (const auto& symbol : symbols) {
            fair_prices_[symbol] = initial_price;
        }
    }

    void tick() override {
        if (symbols_.empty()) {
            return;
        }

        for (const auto& symbol : symbols_) {
            // 1. Get current fair price, potentially update from market
            double current_fair_price = fair_prices_[symbol];

            SymbolID sym_id = SymbolRegistry::getInstance().getID(symbol);
            auto best_bid = engine().getBestBid(sym_id);
            auto best_ask = engine().getBestAsk(sym_id);

            if (best_bid.has_value() && best_ask.has_value()) {
                // Assuming Price is an integer type with 4 decimal places
                double mid_price = (best_bid.value().price + best_ask.value().price) / 2.0 / 10000.0;
                current_fair_price = mid_price;
            }

            // 2. Calculate new fair price using GBM
            double W = norm_dist(gen);
            double new_fair_price = current_fair_price * std::exp((mu_ - 0.5 * sigma_ * sigma_) * dt_ + sigma_ * std::sqrt(dt_) * W);
            fair_prices_[symbol] = new_fair_price;

            // 3. Calculate bid and ask prices
            double bid_price = new_fair_price * (1.0 - spread_);
            double ask_price = new_fair_price * (1.0 + spread_);
            
            // 4. Submit orders
            std::string bid_price_str = format_price(bid_price);
            if (!bid_price_str.empty()) {
                RawOrderParams buy_rop = {
                    .symbol = symbol,
                    .orderType = OrderType::LIMIT,
                    .side = Side::BUY,
                    .price = bid_price_str,
                    .stopPrice = "",
                    .quantity = static_cast<Quantity>(quantity_dist(gen)),
                    .traderID = getID(),
                };
                engine().submitOrder(buy_rop);
            }

            std::string ask_price_str = format_price(ask_price);
            if (!ask_price_str.empty()) {
                RawOrderParams sell_rop = {
                    .symbol = symbol,
                    .orderType = OrderType::LIMIT,
                    .side = Side::SELL,
                    .price = ask_price_str,
                    .stopPrice = "",
                    .quantity = static_cast<Quantity>(quantity_dist(gen)),
                    .traderID = getID(),
                };
                engine().submitOrder(sell_rop);
            }
        }
    }
};