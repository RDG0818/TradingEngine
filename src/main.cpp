#include <iostream>
#include <csignal>
#include <vector>
#include <memory>
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/randomTrader.h"
#include "trading_engine/trendFollowingTrader.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/trader.h"
#include "trading_engine/traderManager.h"

std::atomic<bool> shutdown_flag(false);

void signal_handler(int signum) {
    shutdown_flag = true;
}

class ConsoleLogger : public EventListener {
public:
    void onEvent(const Event& event) override {
        std::cout << "Event: " << event.toString() << std::endl;
    }
};

int main() {
    signal(SIGINT, signal_handler);

    SymbolRegistry::getInstance().registerSymbol("GEMINI");
    SymbolRegistry::getInstance().registerSymbol("RYAN");

    EventDispatcher dispatcher;
    ConsoleLogger logger;
    dispatcher.subscribe(EventType::TRADE_EXECUTED, &logger);
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &logger);
    dispatcher.subscribe(EventType::ORDER_REJECTED, &logger);
    dispatcher.subscribe(EventType::ORDER_CANCELLED, &logger);


    MatchingEngine engine(dispatcher);
    
    TraderManager trader_manager(std::chrono::milliseconds(100));
    trader_manager.addTrader(std::make_unique<RandomTrader>(engine, dispatcher, "GEMINI", std::chrono::milliseconds(500)));
    trader_manager.addTrader(std::make_unique<RandomTrader>(engine, dispatcher, "RYAN", std::chrono::milliseconds(500)));
    trader_manager.addTrader(std::make_unique<TrendFollowingTrader>(engine, dispatcher, "RYAN", std::chrono::milliseconds(1000), 3));


    engine.start();
    trader_manager.start();

    std::cout << "Trading engine and traders started. Press Ctrl+C to stop." << std::endl;

    while (!shutdown_flag) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Shutting down..." << std::endl;

    trader_manager.stop();
    engine.stop();

    std::cout << "Shutdown complete." << std::endl;

    return 0;
}
