// tests/cpp/matchingEngineTest.cpp

#include "gtest/gtest.h"
#include "matchingEngine.h"
#include "orderBook.h"
#include "eventDispatcher.h"
#include "utils.h"
#include "symbolRegistry.h"
#include "orderFactory.h"
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// TODO: Update this to test UpdateBookEvent

class MockEventListener {
public:
    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            trades.push_back(event);
            cv.notify_one();
        });
        dispatcher.subscribe<OrderCancelledEvent>([this](const OrderCancelledEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            cancellations.push_back(event);
            cv.notify_one();
        });
        dispatcher.subscribe<OrderAcceptedEvent>([this](const OrderAcceptedEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            acceptances.push_back(event);
            cv.notify_one();
        });
        dispatcher.subscribe<OrderRejectedEvent>([this](const OrderRejectedEvent& event) {
            std::lock_guard<std::mutex> lock(mtx);
            rejections.push_back(event);
            cv.notify_one();
        });
    }

    void waitForEvents(size_t tradeCount, size_t cancelCount, size_t acceptanceCount = 0, size_t rejectionCount = 0) {
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::seconds(1), [&]{ return trades.size() >= tradeCount && cancellations.size() >= cancelCount && acceptances.size() >= acceptanceCount && rejections.size() >= rejectionCount; })) {
            FAIL() << "Timeout waiting for events. Expected " << tradeCount << " trades, " << cancelCount << " cancellations, " << acceptanceCount << " acceptances, " << rejectionCount << " rejections but got " << trades.size() << ", " << cancellations.size() << ", " << acceptances.size() << ", " << rejections.size();
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        trades.clear();
        cancellations.clear();
        acceptances.clear();
        rejections.clear();
    }

    std::vector<TradeExecutedEvent> getTrades() {
        std::lock_guard<std::mutex> lock(mtx);
        return trades;
    }

    std::vector<OrderCancelledEvent> getCancellations() {
        std::lock_guard<std::mutex> lock(mtx);
        return cancellations;
    }

    std::vector<OrderAcceptedEvent> getAcceptances() {
        std::lock_guard<std::mutex> lock(mtx);
        return acceptances;
    }

    std::vector<OrderRejectedEvent> getRejections() {
        std::lock_guard<std::mutex> lock(mtx);
        return rejections;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<TradeExecutedEvent> trades;
    std::vector<OrderCancelledEvent> cancellations;
    std::vector<OrderAcceptedEvent> acceptances;
    std::vector<OrderRejectedEvent> rejections;
};

class MatchingEngineTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MatchingEngine engine;
    MockEventListener listener;
    SymbolID aapl_id;
    SymbolID goog_id;

    MatchingEngineTest() : engine(dispatcher) {}

    void SetUp() override {
        listener.subscribe(dispatcher);
        engine.start();
        aapl_id = SymbolRegistry::get_instance().get_id("AAPL");
        goog_id = SymbolRegistry::get_instance().get_id("GOOG");
    }

    void TearDown() override {
        engine.stop();
    }
};

TEST_F(MatchingEngineTest, SubmitLimitOrder_NoMatch_RestsOnBook) {
    OrderID orderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 1});
    
    listener.waitForEvents(0, 0, 1, 0);

    auto acceptances = listener.getAcceptances();
    ASSERT_EQ(acceptances.size(), 1);
    EXPECT_EQ(acceptances[0].order_id, orderID);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->get_best_bid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_TRUE(listener.getTrades().empty());
}

TEST_F(MatchingEngineTest, SubmitInvalidOrder_Rejects) {
    OrderID orderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 0, .trader_id = 1});

    listener.waitForEvents(0, 0, 0, 1);

    auto rejections = listener.getRejections();
    ASSERT_EQ(rejections.size(), 1);
    EXPECT_EQ(rejections[0].order_id, orderID);
    EXPECT_EQ(rejections[0].reason, RejectionReason::INVALID_QUANTITY);
}

TEST_F(MatchingEngineTest, SubmitLimitOrders_FullMatch) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    OrderID aggressingID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 2});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 1000000);
    EXPECT_EQ(trades[0].quantity, 10);
    EXPECT_EQ(trades[0].aggressing_order_id, aggressingID);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_empty());
}

TEST_F(MatchingEngineTest, SubmitLimitOrder_PartialMatch_RestsOnBook) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 2});

    listener.waitForEvents(1, 0, 2, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->get_best_bid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 5);
}

TEST_F(MatchingEngineTest, SubmitMarketOrder_FullMatch) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::MARKET, .side = Side::BUY, .price = "", .quantity = 10, .trader_id = 2});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_empty());
}

TEST_F(MatchingEngineTest, SubmitMarketOrder_PartialMatch_RemainderCancelled) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    OrderID marketOrderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::MARKET, .side = Side::BUY, .price = "", .quantity = 10, .trader_id = 2});

    listener.waitForEvents(1, 1, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].order_id, marketOrderID);
    EXPECT_EQ(cancellations[0].quantity, 5);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_empty());
}

TEST_F(MatchingEngineTest, CancelOrder_RemovesFromBook) {
    OrderID orderToCancel = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.cancel_order(orderToCancel);

    listener.waitForEvents(0, 1, 1, 0);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].order_id, orderToCancel);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_side_empty(Side::BUY));
}

TEST_F(MatchingEngineTest, PriceTimePriority_IsRespected) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 5, .trader_id = 1});
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 2});
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 3});
    listener.waitForEvents(0, 0, 3, 0);

    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "101.00", .quantity = 12, .trader_id = 4});

    listener.waitForEvents(3, 0, 3, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 3);

    // First trade should be with the best price (100.00) and first in time (trader 2)
    EXPECT_EQ(trades[0].price, 1000000);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[0].resting_trader_id, 2);

    // Second trade should be with the same price (100.00) and second in time (trader 3)
    EXPECT_EQ(trades[1].price, 1000000);
    EXPECT_EQ(trades[1].quantity, 5);
    EXPECT_EQ(trades[1].resting_trader_id, 3);

    // Third trade should be with the next best price (101.00)
    EXPECT_EQ(trades[2].price, 1010000);
    EXPECT_EQ(trades[2].quantity, 2);
    EXPECT_EQ(trades[2].resting_trader_id, 1);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->get_best_ask();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 1010000);
    EXPECT_EQ(bestAsk->quantity, 3);
}

TEST_F(MatchingEngineTest, StopMarketOrder_TriggersAndFills) {
    // Resting sell order
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "105.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);

    // Buy stop order, should trigger when price goes >= 100.00
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::STOP_MARKET, .side = Side::BUY, .stop_price = "100.00", .quantity = 10, .trader_id = 2});

    // A trade that should not trigger the stop
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "99.00", .quantity = 1, .trader_id = 3});
    listener.waitForEvents(0, 0, 2, 0);
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 1, .trader_id = 4});
    listener.waitForEvents(1, 0, 2, 0);
    EXPECT_EQ(listener.getTrades().size(), 1);

    // This trade should trigger the stop order
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 5});
    listener.waitForEvents(1, 0, 3, 0);
    OrderID aggressingID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 5, .trader_id = 6});
    
    listener.waitForEvents(3, 0, 3, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 3);

    // The third trade should be the triggered stop order filling against the resting order
    EXPECT_EQ(trades[2].price, 1050000);
    EXPECT_EQ(trades[2].quantity, 10);
}

TEST_F(MatchingEngineTest, StopLimitOrder_TriggersAndRests) {
    // Buy stop limit order, should trigger when price goes >= 100.00 and then rest as a limit order at 101.00
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::STOP_LIMIT, .side = Side::BUY, .price = "101.00", .stop_price = "100.00", .quantity = 10, .trader_id = 1});

    // This trade should trigger the stop order
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 2});
    listener.waitForEvents(0, 0, 1, 0);
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 5, .trader_id = 3});
    
    listener.waitForEvents(1, 0, 2, 0);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->get_best_bid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1010000);
    EXPECT_EQ(bestBid->quantity, 10);
}

TEST_F(MatchingEngineTest, LimitOrderIOC_PartialFill_CancelsRemainder) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);
    OrderID iocOrderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 2, .time_in_force = TimeInForce::IOC});

    listener.waitForEvents(1, 1, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].order_id, iocOrderID);
    EXPECT_EQ(cancellations[0].quantity, 5);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_empty());
}

TEST_F(MatchingEngineTest, LimitOrderFOK_FullFill_Executes) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 2, .time_in_force = TimeInForce::FOK});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->is_empty());
}

TEST_F(MatchingEngineTest, LimitOrderFOK_PartialFill_Cancels) {
    engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);
    OrderID fokOrderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 2, .time_in_force = TimeInForce::FOK});

    listener.waitForEvents(0, 1, 1, 0);

    EXPECT_TRUE(listener.getTrades().empty());

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].order_id, fokOrderID);
    EXPECT_EQ(cancellations[0].quantity, 10);

    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->get_best_ask();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 1000000);
    EXPECT_EQ(bestAsk->quantity, 5);
}

TEST_F(MatchingEngineTest, SelfMatchPrevention_RestingOrderCancelled) {
    // Trader 1 places a SELL limit order
    OrderID restingSellOrderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .trader_id = 1});
    listener.waitForEvents(0, 0, 1, 0);
    listener.clear(); // Clear events to only capture new events

    // Trader 1 places a BUY limit order at the same price (should trigger self-match prevention)
    OrderID incomingBuyOrderID = engine.submit_order({.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .trader_id = 1});
    
    // Expect 1 cancellation (resting sell order) and 1 acceptance (incoming buy order)
    listener.waitForEvents(0, 1, 1, 0); 

    // Verify no trades occurred
    EXPECT_TRUE(listener.getTrades().empty());

    // Verify the resting sell order was cancelled
    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].order_id, restingSellOrderID);
    EXPECT_EQ(cancellations[0].trader_id, 1);
    EXPECT_EQ(cancellations[0].quantity, 10);

    // Verify the incoming buy order was accepted
    auto acceptances = listener.getAcceptances();
    ASSERT_EQ(acceptances.size(), 1);
    EXPECT_EQ(acceptances[0].order_id, incomingBuyOrderID);
    EXPECT_EQ(acceptances[0].trader_id, 1);

    // Verify the incoming buy order is now resting on the book
    OrderBook* book = engine.get_book(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->get_best_bid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_TRUE(book->is_side_empty(Side::SELL)); // The sell side should be empty as the resting order was cancelled
}
