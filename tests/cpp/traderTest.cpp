#include "trading_engine/trader.h"
#include "gtest/gtest.h"
#include "trading_engine/symbolRegistry.h"
#include <memory>
#include <vector>
#include <mutex>
#include <iostream>
#include <chrono>

class traderTest : public ::testing::Test {
private:

protected:

    EventDispatcher dispatcher;
    MatchingEngine engine;
    std::chrono::milliseconds tickInterval = std::chrono::milliseconds(100);
    TraderManager manager;
    LiquidityTrader* liquidTrader_ptr = nullptr;

    // Member variables to hold test results, moved from test body.
    std::vector<TradeExecutedEvent> trade_events;
    std::mutex events_mutex;

public:

    traderTest () : engine(dispatcher), manager(tickInterval) {};

    void SetUp() override {
        // Subscribe to events BEFORE starting the components that will generate them
        // to prevent a race condition where events are published before subscription.
        dispatcher.subscribe<TradeExecutedEvent>(
            [&](const TradeExecutedEvent& event) {
                std::lock_guard<std::mutex> lock(events_mutex);
                // We only care about trades where our trader was the aggressor
                if (liquidTrader_ptr && event.aggressingTraderID == liquidTrader_ptr->getID()) {
                    trade_events.push_back(event);
                }
            }
        );

        engine.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 

        engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 100000, .traderID = 999});
        engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 100000, .traderID = 998});
        engine.submitOrder({.symbol = "GOOG", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "50.00", .quantity = 100000, .traderID = 997});
        engine.submitOrder({.symbol = "GOOG", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "49.00", .quantity = 100000, .traderID = 996});
        // Use a high lambda to ensure orders are submitted quickly for a deterministic test.
        auto liquidTrader = std::make_unique<LiquidityTrader>(
            engine,
            dispatcher,
            0,
            10.0,
            tickInterval,
            std::vector<std::string>{"AAPL", "GOOG"},
            5
        );
        liquidTrader_ptr = liquidTrader.get();
        manager.addTrader(std::move(liquidTrader));
        
        // Start the manager AFTER subscribing.
        manager.start();
        
        // Sleep to allow the system to warm up and process initial trades.
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }

    void TearDown() override {
        manager.stop();
        engine.stop();
    }
};

TEST_F(traderTest, LiquidityTraderSubmitsOrder) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager.stop();
    engine.stop(); 

    std::cout << "Received " << trade_events.size() << " TradeExecutedEvents from the LiquidityTrader." << std::endl;

    ASSERT_FALSE(trade_events.empty()) << "No trades were executed by the LiquidityTrader.";

    // Optional: further checks on the trade event
    const auto& first_trade = trade_events.front();
    ASSERT_EQ(first_trade.aggressingTraderID, liquidTrader_ptr->getID());
    
    SymbolID aapl_id = SymbolRegistry::getInstance().getID("AAPL");
    SymbolID goog_id = SymbolRegistry::getInstance().getID("GOOG");

    ASSERT_TRUE(first_trade.symbolID == aapl_id || first_trade.symbolID == goog_id)
        << "Trade was not for an expected symbol.";
}


