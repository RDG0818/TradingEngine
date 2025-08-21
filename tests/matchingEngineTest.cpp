
#include "gtest/gtest.h"
#include "matchingEngine.h"
#include "orderBook.h"
#include "eventDispatcher.h"
#include "limitOrder.h"
#include "marketOrder.h"
#include "types.h"
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

    // Tell the listener how many events to wait for
    void setExpectedEvents(int count) {
        expected_events = count;
    }

    // Block the test thread until the expected number of events arrive
    void waitForEvents() {
        // Use a timeout to prevent tests from hanging indefinitely
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::milliseconds(500), [this]{ return expected_events <= 0; })) {
            // If the timeout is reached, fail the test
            FAIL() << "Timeout waiting for events. Expected " << expected_events.load() << " more events.";
        }
    }
};


// --- Test Fixture for the MatchingEngine ---
class MatchingEngineTest : public ::testing::Test {
protected:
    // These are recreated for each test
    OrderBook book;
    EventDispatcher dispatcher;
    MatchingEngine engine;
    MockListener listener;
    
    MatchingEngineTest() : engine(book, dispatcher) {}

    void SetUp() override {
        listener.subscribe(dispatcher);
        engine.start();
    }
    
    void TearDown() override {
        engine.stop();
    }
    
     OrderID submitOrderWithUnblock(std::unique_ptr<Order> order) {
        OrderID id = engine.submitOrder(std::move(order));
        engine.cancelOrder(0); // Push sentinel to unblock cancellation queue
        return id;
    }
};


// --- NEW TEST CASE for Cancellation ---
TEST_F(MatchingEngineTest, CancelOrder_RemovesOrderFromBook) {
    // ARRANGE: Submit a limit order that will rest on the book.
    auto sellOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "101.00", 10, 2);
    OrderID restingOrderID = engine.submitOrder(std::move(sellOrder));

    // Allow time for the order to be processed and placed on the book.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // VERIFY PRE-CONDITION: The order is on the book.
    auto bestAskBeforeCancel = book.getBestAsk();
    ASSERT_TRUE(bestAskBeforeCancel.has_value());
    EXPECT_EQ(bestAskBeforeCancel->price, 10100);

    // ACT: Cancel the order.
    engine.cancelOrder(restingOrderID);

    // Allow time for the cancellation to be processed.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // ASSERT: The order should no longer be on the book.
    auto bestAskAfterCancel = book.getBestAsk();
    EXPECT_FALSE(bestAskAfterCancel.has_value());
    EXPECT_TRUE(listener.trades.empty()); // Ensure no trades occurred.
}


// --- EXISTING TEST CASES (Updated) ---

TEST_F(MatchingEngineTest, LimitOrder_NoMatch_RestsOnBook) {
    auto buyOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::BUY, "99.00", 10, 1);
    
    // Pre-load the book with a sell order that won't cross
    auto sellOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "101.00", 10, 2);
    submitOrderWithUnblock(std::move(sellOrder));

    // Process the buy order
    submitOrderWithUnblock(std::move(buyOrder));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // VERIFY: No trades occurred
    EXPECT_TRUE(listener.trades.empty());
    
    // VERIFY: Both orders are now resting on the book
    auto bestBid = book.getBestBid();
    auto bestAsk = book.getBestAsk();
    ASSERT_TRUE(bestBid.has_value());
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestBid->price, 9900);
    EXPECT_EQ(bestAsk->price, 10100);
}

TEST_F(MatchingEngineTest, LimitOrder_PartialFill_ThenRests) {
    // Resting sell order for 10 shares at $100
    listener.setExpectedEvents(1);
    auto sellOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2);
    submitOrderWithUnblock(std::move(sellOrder));

    // Aggressing buy order for 15 shares at $100
    auto buyOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::BUY, "100.00", 15, 1);
    // Get a raw pointer to check the order's state after submission.
    // The engine takes ownership, so we can't use the unique_ptr after moving it.
    Order* buyOrderPtr = buyOrder.get();
    submitOrderWithUnblock(std::move(buyOrder));
    
    listener.waitForEvents();

    // VERIFY: One trade occurred for 10 shares
    ASSERT_EQ(listener.trades.size(), 1);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 10000);

    // VERIFY: The aggressing buy order has 5 shares remaining and is now on the book
    // We need to get the order from the book now, as the original pointer might be stale
    // if the order object was moved. The OrderID is the reliable way to track it.
    auto bestBid = book.getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 5);

    // VERIFY: The sell side of the book is now empty
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, LimitOrder_WalkTheBook_FullFill) {
    // Pre-load the book with multiple sell orders
    listener.setExpectedEvents(2);
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2));
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "101.00", 10, 3));

    // Aggressing buy order for 15 shares, willing to pay up to $101
    auto buyOrder = std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::BUY, "101.00", 15, 1);
    submitOrderWithUnblock(std::move(buyOrder));

    listener.waitForEvents();
    // VERIFY: Two trades occurred
    ASSERT_EQ(listener.trades.size(), 2);
    // Trade 1: 10 shares at $100
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 10000);
    // Trade 2: 5 shares at $101
    EXPECT_EQ(listener.trades[1].quantity, 5);
    EXPECT_EQ(listener.trades[1].price, 10100);

    // VERIFY: The book now has one sell order with 5 shares remaining
    auto bestAsk = book.getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 10100);
    EXPECT_EQ(bestAsk->quantity, 5);
}

TEST_F(MatchingEngineTest, MarketOrder_ClearsBook_RemainderCancelled) {
    // Pre-load the book with 15 shares total to sell
    listener.setExpectedEvents(3); // 2 trades + 1 cancellation
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2));
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "101.00", 5, 3));

    // Aggressing market order to buy 20 shares
    auto buyOrder = std::make_unique<MarketOrder>("AAPL", 0, OrderType::MARKET, Side::BUY, 20, 1);
    OrderID aggressingID = engine.submitOrder(std::move(buyOrder));

    listener.waitForEvents();
    // VERIFY: Two trades occurred, clearing the book
    ASSERT_EQ(listener.trades.size(), 2);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[1].quantity, 5);

    // VERIFY: The remaining 5 shares of the MARKET order were cancelled
    ASSERT_EQ(listener.cancellations.size(), 1);
    EXPECT_EQ(listener.cancellations[0].orderID, aggressingID);
    EXPECT_EQ(listener.cancellations[0].quantity, 5);

    // VERIFY: The sell side of the book is now empty
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, PricePriority_IsRespected_InAsynchronousProcessing) {
    listener.setExpectedEvents(2); 
    // Order A: Submitted first, but at a worse price.
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "101.00", 10, 1)); 
    
    // Order B: Submitted second, but at a better price. This one should trade first.
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2)); 
    
    // Order C: The aggressing order that will trigger the trades.
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::BUY, "101.00", 15, 3));  

    listener.waitForEvents();

    // VERIFY: Two trades occurred
    ASSERT_EQ(listener.trades.size(), 2);

    // VERIFY: The first trade was against the best-priced order (Order B at $100)
    EXPECT_EQ(listener.trades[0].price, 10000); 
    EXPECT_EQ(listener.trades[0].quantity, 10);

    // VERIFY: The second trade was against the remaining order (Order A at $101)
    EXPECT_EQ(listener.trades[1].price, 10100); 
    EXPECT_EQ(listener.trades[1].quantity, 5);
}

TEST_F(MatchingEngineTest, ConcurrentSubmissions_AreHandledSafely) {
    // Pre-load the book with a large sell order from Trader 1
    listener.setExpectedEvents(100);
    submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::SELL, "100.00", 1000, 1));

    // Create 10 threads that will each submit 10 buy orders from other traders
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            int traderId = 10 + i; 
            for (int j = 0; j < 10; ++j) {
                submitOrderWithUnblock(std::make_unique<LimitOrder>("AAPL", 0, OrderType::LIMIT, Side::BUY, "100.00", 1, traderId));
            }
        });
    }

    // Wait for all submission threads to complete
    for (auto& t : threads) {
        t.join();
    }

    listener.waitForEvents();

    // VERIFY: 100 trades should have occurred (10 threads * 10 orders)
    ASSERT_EQ(listener.trades.size(), 100);

    // VERIFY: The resting order should have been reduced by 100 shares
    auto bestAsk = book.getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->quantity, 900); // 1000 - (10 * 10 * 1)
}
