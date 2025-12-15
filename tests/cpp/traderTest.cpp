#include "trading_engine/trader.h"
#include "gtest/gtest.h"
#include "trading_engine/symbolRegistry.h"
#include <memory>
#include <vector>
#include <mutex>
#include <iostream>
#include <chrono>
#include <unordered_map>

class traderTest : public ::testing::Test {
private:

protected:

    EventDispatcher dispatcher;
    MatchingEngine engine;
    std::chrono::milliseconds tickInterval = std::chrono::milliseconds(100);
    TraderManager manager;
    LiquidityTrader* liquidTrader_ptr = nullptr;
    RandomTrader* randomTrader_ptr = nullptr;
    MarketMakerTrader* marketMakerTrader_ptr = nullptr;

    // Member variables to hold test results, moved from test body.
    std::vector<TradeExecutedEvent> trade_events;
    std::vector<OrderAcceptedEvent> accepted_events;
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

        dispatcher.subscribe<OrderAcceptedEvent>(
            [&](const OrderAcceptedEvent& event) {
                std::lock_guard<std::mutex> lock(events_mutex);
                accepted_events.push_back(event);
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
            0, // TraderID
            1000.0,
            tickInterval,
            std::vector<std::string>{"AAPL", "GOOG"},
            5 // Max quantity
        );
        liquidTrader_ptr = liquidTrader.get();
        manager.addTrader(std::move(liquidTrader));
        
        auto randomTrader = std::make_unique<RandomTrader>(
            engine,
            dispatcher,
            1,
            10000.0,
            tickInterval,
            std::vector<std::string>{"AAPL", "GOOG"},
            10,
            0.05 // Normal Dist Variation
        );
        randomTrader_ptr = randomTrader.get();
        manager.addTrader(std::move(randomTrader));

        auto marketMakerTrader = std::make_unique<MarketMakerTrader>(
            engine,
            dispatcher,
            2, // TraderID
            std::vector<std::string>{"AAPL", "GOOG"},
            0.0, // mu
            0.1, // sigma
            0.001, // spread
            tickInterval,
            10, // max quantity
            std::unordered_map<std::string, double>{{"AAPL", 100.0}, {"GOOG", 50.0}} // initial prices
        );
        marketMakerTrader_ptr = marketMakerTrader.get();
        manager.addTrader(std::move(marketMakerTrader));

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
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

TEST_F(traderTest, RandomTraderSubmitsOrder) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int num_accepted_events = 0;
    for (auto event : accepted_events) {
        if (event.traderID == 1) {// RandomTrader's ID
            num_accepted_events++;
        }
    }

    std::cout << "Received " << num_accepted_events << " OrderAcceptedEvents from the RandomTrader." << std::endl;
    ASSERT_FALSE(num_accepted_events == 0) << "No trades were executed by the RandomTrader.";

    engine.printTopOfBook("AAPL", 10);
    engine.printTopOfBook("GOOG", 10);

}

TEST_F(traderTest, MarketMakerTraderSubmitsOrders) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager.stop();
    engine.stop();

    int mm_accepted_orders = 0;
    bool found_buy = false;
    bool found_sell = false;
    for (const auto& event : accepted_events) {
        if (event.traderID == marketMakerTrader_ptr->getID()) {
            mm_accepted_orders++;
            if (event.side == Side::BUY) {
                found_buy = true;
            }
            if (event.side == Side::SELL) {
                found_sell = true;
            }
        }
    }

    std::cout << "Received " << mm_accepted_orders << " OrderAcceptedEvents from the MarketMakerTrader." << std::endl;
    ASSERT_GT(mm_accepted_orders, 0) << "MarketMakerTrader did not submit any orders.";
    ASSERT_TRUE(found_buy) << "MarketMakerTrader did not submit any BUY orders.";
    ASSERT_TRUE(found_sell) << "MarketMakerTrader did not submit any SELL orders.";

    engine.printTopOfBook("AAPL", 10);
    engine.printTopOfBook("GOOG", 10);
}

