#include "matchingEngine.h"
#include "trader.h"
#include <iostream>
#include <signal.h>
#include <atomic>

// TODO: Add input validation
// TODO: Add further configurations for each trader
// TODO: Figure out how to replace top of orderbook instead of just print
// TODO: Wrap the whole thing in python and make it a backend

std::atomic<bool> isRunning(true);
void signal_handler(int signal) {
    isRunning.store(false);
}

int main() {

    signal(SIGINT, signal_handler);

    // Initialization
    EventDispatcher dispatcher;
    MatchingEngine engine(dispatcher);
    std::chrono::milliseconds tickInterval = std::chrono::milliseconds(10);
    TraderManager manager(tickInterval);
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

    // Creating Bots
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> lambda_dist(1, 10); // TODO: Hardcoded for now

    int i = 0;
    for (; i < numLiquidTraders; i++) {
        auto liquidTrader = std::make_unique<LiquidityTrader>(
            engine,
            dispatcher,
            i, // TraderID
            static_cast<float>(lambda_dist(gen)),
            tickInterval,
            symbols,
            10 // Max quantity
        );
        manager.addTrader(std::move(liquidTrader));
    }

    for (; i < numLiquidTraders + numRandomTraders; i++) {
        auto randomTrader = std::make_unique<RandomTrader>(
            engine,
            dispatcher,
            i,
            static_cast<float>(lambda_dist(gen)),
            tickInterval,
            symbols,
            10,
            0.15 // Normal Dist Variation
        );
        manager.addTrader(std::move(randomTrader));
    }

    for (; i < numLiquidTraders + numRandomTraders + numMarketMakerTraders; i++) {
        auto marketMakerTrader = std::make_unique<MarketMakerTrader>(
            engine,
            dispatcher,
            i, // TraderID
            symbols,
            0.0, // mu
            0.025, // sigma
            0.001, // spread
            tickInterval,
            10, // max quantity
            initialPrices // initial prices
        );
        manager.addTrader(std::move(marketMakerTrader));
    }

    engine.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); 

    std::cout << "Press CTRL + C to end program.\n";

    while (isRunning) {
        for (std::string sym : symbols) engine.printTopOfBook(sym, 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }

    engine.stop();
    manager.stop();

    return 0;
}