// src/core/cli.cpp
#include "matchingEngine.h"
#include "trader.h"
#include "traderManager.h"
#include <iostream>
#include <signal.h>
#include <atomic>

// This file is intended as an example use case of the trading engine and virtual traders

// TODO: Add input validation
// TODO: Add further configurations for each trader
// TODO: Figure out how to replace top of orderbook instead of just print

std::atomic<bool> isRunning(true);
void signal_handler(int signal) {
    isRunning.store(false);
}

int main() {

    signal(SIGINT, signal_handler);

    // Initialization
    EventDispatcher dispatcher;
    OrderIdGenerator order_id_generator;
    MatchingEngine engine(dispatcher, order_id_generator);
    std::chrono::milliseconds tickInterval = std::chrono::milliseconds(10);
    std::cout << "How many random market traders in the market?\n";
    int numLiquidTraders; std::cin >> numLiquidTraders;

    std::cout << "How many random limit traders in the market?\n";
    int numRandomTraders; std::cin >> numRandomTraders;

    std::cout << "How many market maker traders in the market?\n";
    int numMarketMakerTraders; std::cin >> numMarketMakerTraders;

    std::cout << "How many symbols do you want to be traded?\n";
    int numSymbols; std::cin >> numSymbols;
    std::cout << "What symbols do you want to be traded?\n";
    std::vector<std::string> symbols(numSymbols); for(int i=0;i<numSymbols;i++) std::cin >> symbols[i];
    std::cout << "What are the initial prices of each symbol?\n";
    std::unordered_map<std::string, double> initialPrices; for(int i=0;i<numSymbols;i++) std::cin >> initialPrices[symbols[i]];

    TraderManager manager(engine, dispatcher, tickInterval, symbols);

    // Creating Bots
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> lambda_dist(1, 10); // TODO: Hardcoded for now

    int i = 0;
    for (; i < numLiquidTraders; i++) {
        manager.addRandomMarketTrader(
            std::to_string(i),
            static_cast<float>(lambda_dist(gen)),
            10
        );
    }

    for (; i < numLiquidTraders + numRandomTraders; i++) {
        manager.addRandomLimitTrader(
            std::to_string(i),
            static_cast<float>(lambda_dist(gen)),
            10,
            0.15
        );
    }

    for (; i < numLiquidTraders + numRandomTraders + numMarketMakerTraders; i++) {
        manager.addMarketMakerTrader(
            std::to_string(i),
            0.0,
            0.025,
            0.001,
            10,
            initialPrices
        );
    }

    engine.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); 

    std::cout << "Press CTRL + C to end program.\n";

    while (isRunning) {
        for (std::string sym : symbols) engine.print_top_of_book(sym, 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }

    engine.stop();
    manager.stop();

    return 0;
}