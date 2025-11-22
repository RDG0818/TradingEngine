#include "gtest/gtest.h"
#include "trading_engine/trendFollowingTrader.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/events.h"
#include "trading_engine/symbolRegistry.h"
#include <memory>

class TrendFollowingTraderTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MatchingEngine engine;
    SymbolID symbol_id;

    TrendFollowingTraderTest() : engine(dispatcher) {
        symbol_id = SymbolRegistry::getInstance().registerSymbol("TREND");
    }

    void SetUp() override {
        engine.start();
    }

    void TearDown() override {
        engine.stop();
    }
};

class OrderCreationListener : public EventListener {
public:
    std::atomic<int> trade_count{0};
    Side last_aggressor_side;
    std::mutex mtx;
    std::condition_variable cv;

    void onEvent(const Event& event) override {
        if (event.type == EventType::TRADE_EXECUTED) {
            std::lock_guard<std::mutex> lock(mtx);
            const auto& trade_event = static_cast<const TradeExecutedEvent&>(event);
            if (trade_event.aggressorTraderID == 200) {
                last_aggressor_side = trade_event.aggressorSide;
                trade_count++;
                cv.notify_one();
            }
        }
    }

    void waitForTrades(int expected_trades, Side expected_side) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&]{ return trade_count >= expected_trades && last_aggressor_side == expected_side; });
    }
};

TEST_F(TrendFollowingTraderTest, IdentifiesUptrendAndBuys) {
    OrderCreationListener trade_listener;
    dispatcher.subscribe(EventType::TRADE_EXECUTED, &trade_listener);

    TrendFollowingTrader trader(engine, dispatcher, "TREND", std::chrono::milliseconds(100), 3);
    trader.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate an uptrend
    engine.submitOrder(RawOrderParams{"TREND", 3, Side::SELL, OrderType::LIMIT, 100, 103});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 100, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 101, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 102, 1, 1, 1, Side::BUY, 0, 2, 2, 0});

    trade_listener.waitForTrades(1, Side::BUY);

    EXPECT_EQ(trade_listener.trade_count, 1);
    EXPECT_EQ(trade_listener.last_aggressor_side, Side::BUY);


}

TEST_F(TrendFollowingTraderTest, IdentifiesDowntrendAndSells) {
    OrderCreationListener trade_listener;
    dispatcher.subscribe(EventType::TRADE_EXECUTED, &trade_listener);

    TrendFollowingTrader trader(engine, dispatcher, "TREND", std::chrono::milliseconds(100), 3);
    trader.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate a downtrend
    engine.submitOrder(RawOrderParams{"TREND", 3, Side::BUY, OrderType::LIMIT, 100, 99});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 102, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 101, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 100, 1, 1, 1, Side::BUY, 0, 2, 2, 0});

    trade_listener.waitForTrades(1, Side::SELL);

    EXPECT_EQ(trade_listener.trade_count, 1);
    EXPECT_EQ(trade_listener.last_aggressor_side, Side::SELL);


}

TEST_F(TrendFollowingTraderTest, NoTrendNoAction) {
    OrderCreationListener trade_listener;
    dispatcher.subscribe(EventType::TRADE_EXECUTED, &trade_listener);

    TrendFollowingTrader trader(engine, dispatcher, "TREND", std::chrono::milliseconds(100), 3);
    trader.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate no trend
    dispatcher.publish(TradeExecutedEvent{symbol_id, 101, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 102, 1, 1, 1, Side::BUY, 0, 2, 2, 0});
    dispatcher.publish(TradeExecutedEvent{symbol_id, 101, 1, 1, 1, Side::BUY, 0, 2, 2, 0});

    // No trades are expected, so we don't wait.
    // A small sleep to allow the trader to run at least once.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(trade_listener.trade_count, 0);


}