#include "gtest/gtest.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderBook.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/limitOrder.h"
#include "trading_engine/marketOrder.h"
#include "trading_engine/types.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/orderFactory.h"
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class MockListener {
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<int> expected_events{0};

public:
    std::vector<TradeExecutedEvent> trades;
    std::vector<OrderCancelledEvent> cancellations;

    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            trades.push_back(event);
            if (--expected_events <= 0) {
                cv.notify_one();
            }
        });
        dispatcher.subscribe<OrderCancelledEvent>([this](const OrderCancelledEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            cancellations.push_back(event);
            if (--expected_events <= 0) {
                cv.notify_one();
            }
        });
    }

    void setExpectedEvents(int count) {
        expected_events = count;
    }

    void waitForEvents() {
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::milliseconds(500), [this]{ return expected_events <= 0; })) {
            FAIL() << "Timeout waiting for events. Expected " << expected_events.load() << " more events.";
        }
    }
};

class MatchingEngineTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MatchingEngine engine;
    MockListener listener;
    SymbolID aapl_id;
    
    MatchingEngineTest() : engine(dispatcher) {}

    void SetUp() override {
        listener.subscribe(dispatcher);
        engine.start();
        aapl_id = SymbolRegistry::getInstance().getID("AAPL");
    }
    
    void TearDown() override {
        engine.stop();
    }
};

TEST_F(MatchingEngineTest, CancelOrder_RemovesOrderFromBook) {
    RawOrderParams params = {.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 10, .traderID = 2};
    OrderID restingOrderID = engine.submitOrder(params);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);

    auto bestAskBeforeCancel = book->getBestAsk();
    ASSERT_TRUE(bestAskBeforeCancel.has_value());
    EXPECT_EQ(bestAskBeforeCancel->price, 1010000);

    engine.cancelOrder(restingOrderID);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto bestAskAfterCancel = book->getBestAsk();
    EXPECT_FALSE(bestAskAfterCancel.has_value());
    EXPECT_TRUE(listener.trades.empty());
}

TEST_F(MatchingEngineTest, LimitOrder_NoMatch_RestsOnBook) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 10, .traderID = 2});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 10, .traderID = 1});

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    EXPECT_TRUE(listener.trades.empty());
    
    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    auto bestAsk = book->getBestAsk();
    ASSERT_TRUE(bestBid.has_value());
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestBid->price, 990000);
    EXPECT_EQ(bestAsk->price, 1010000);
}

TEST_F(MatchingEngineTest, LimitOrder_PartialFill_ThenRests) {
    listener.setExpectedEvents(1);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 2});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 15, .traderID = 1});
    
    listener.waitForEvents();

    ASSERT_EQ(listener.trades.size(), 1);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 1000000);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 5);

    EXPECT_FALSE(book->getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, LimitOrder_WalkTheBook_FullFill) {
    listener.setExpectedEvents(2);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 2});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 10, .traderID = 3});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "101.00", .quantity = 15, .traderID = 1});

    listener.waitForEvents();
    
    ASSERT_EQ(listener.trades.size(), 2);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 1000000);
    EXPECT_EQ(listener.trades[1].quantity, 5);
    EXPECT_EQ(listener.trades[1].price, 1010000);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 1010000);
    EXPECT_EQ(bestAsk->quantity, 5);
}

TEST_F(MatchingEngineTest, MarketOrder_ClearsBook_RemainderCancelled) {
    listener.setExpectedEvents(3);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 2});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 5, .traderID = 3});

    OrderID aggressingID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::MARKET, .side = Side::BUY, .price = "", .quantity = 20, .traderID = 1});

    listener.waitForEvents();
    
    ASSERT_EQ(listener.trades.size(), 2);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[1].quantity, 5);

    ASSERT_EQ(listener.cancellations.size(), 1);
    EXPECT_EQ(listener.cancellations[0].orderID, aggressingID);
    EXPECT_EQ(listener.cancellations[0].quantity, 5);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_FALSE(book->getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, PricePriority_IsRespected_InAsynchronousProcessing) {
    listener.setExpectedEvents(2); 
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 10, .traderID = 1}); 
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 2}); 
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "101.00", .quantity = 15, .traderID = 3});  

    listener.waitForEvents();

    ASSERT_EQ(listener.trades.size(), 2);
    EXPECT_EQ(listener.trades[0].price, 1000000); 
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[1].price, 1010000); 
    EXPECT_EQ(listener.trades[1].quantity, 5);
}

TEST_F(MatchingEngineTest, ConcurrentSubmissions_AreHandledSafely) {
    listener.setExpectedEvents(100);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 1000, .traderID = 1});

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            int traderId = 10 + i; 
            for (int j = 0; j < 10; ++j) {
                engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 1, .traderID = traderId});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    listener.waitForEvents();

    ASSERT_EQ(listener.trades.size(), 100);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->quantity, 900);
}
