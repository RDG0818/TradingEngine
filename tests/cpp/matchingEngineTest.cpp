#include "gtest/gtest.h"
#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderBook.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/utils.h"
#include "trading_engine/symbolRegistry.h"
#include "trading_engine/orderFactory.h"
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

class MatchingEngineTestV2 : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MatchingEngine engine;
    MockEventListener listener;
    SymbolID aapl_id;
    SymbolID goog_id;

    MatchingEngineTestV2() : engine(dispatcher) {}

    void SetUp() override {
        listener.subscribe(dispatcher);
        engine.start();
        aapl_id = SymbolRegistry::getInstance().getID("AAPL");
        goog_id = SymbolRegistry::getInstance().getID("GOOG");
    }

    void TearDown() override {
        engine.stop();
    }
};

TEST_F(MatchingEngineTestV2, SubmitLimitOrder_NoMatch_RestsOnBook) {
    OrderID orderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 1});
    
    listener.waitForEvents(0, 0, 1, 0);

    auto acceptances = listener.getAcceptances();
    ASSERT_EQ(acceptances.size(), 1);
    EXPECT_EQ(acceptances[0].orderID, orderID);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_TRUE(listener.getTrades().empty());
}

TEST_F(MatchingEngineTestV2, SubmitInvalidOrder_Rejects) {
    OrderID orderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 0, .traderID = 1});

    listener.waitForEvents(0, 0, 0, 1);

    auto rejections = listener.getRejections();
    ASSERT_EQ(rejections.size(), 1);
    EXPECT_EQ(rejections[0].orderID, orderID);
    EXPECT_EQ(rejections[0].reason, RejectionReason::INVALID_QUANTITY);
}

TEST_F(MatchingEngineTestV2, SubmitLimitOrders_FullMatch) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    OrderID aggressingID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 2});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 1000000);
    EXPECT_EQ(trades[0].quantity, 10);
    EXPECT_EQ(trades[0].aggressingOrderID, aggressingID);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isEmpty());
}

TEST_F(MatchingEngineTestV2, SubmitLimitOrder_PartialMatch_RestsOnBook) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 2});

    listener.waitForEvents(1, 0, 2, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 5);
}

TEST_F(MatchingEngineTestV2, SubmitMarketOrder_FullMatch) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::MARKET, .side = Side::BUY, .price = "", .quantity = 10, .traderID = 2});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isEmpty());
}

TEST_F(MatchingEngineTestV2, SubmitMarketOrder_PartialMatch_RemainderCancelled) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    OrderID marketOrderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::MARKET, .side = Side::BUY, .price = "", .quantity = 10, .traderID = 2});

    listener.waitForEvents(1, 1, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].orderID, marketOrderID);
    EXPECT_EQ(cancellations[0].quantity, 5);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isEmpty());
}

TEST_F(MatchingEngineTestV2, CancelOrder_RemovesFromBook) {
    OrderID orderToCancel = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    engine.cancelOrder(orderToCancel);

    listener.waitForEvents(0, 1, 1, 0);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].orderID, orderToCancel);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isSideEmpty(Side::BUY));
}

TEST_F(MatchingEngineTestV2, PriceTimePriority_IsRespected) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "101.00", .quantity = 5, .traderID = 1});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 2});
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 3});
    listener.waitForEvents(0, 0, 3, 0);

    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "101.00", .quantity = 12, .traderID = 4});

    listener.waitForEvents(3, 0, 3, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 3);

    // First trade should be with the best price (100.00) and first in time (trader 2)
    EXPECT_EQ(trades[0].price, 1000000);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[0].restingTraderID, 2);

    // Second trade should be with the same price (100.00) and second in time (trader 3)
    EXPECT_EQ(trades[1].price, 1000000);
    EXPECT_EQ(trades[1].quantity, 5);
    EXPECT_EQ(trades[1].restingTraderID, 3);

    // Third trade should be with the next best price (101.00)
    EXPECT_EQ(trades[2].price, 1010000);
    EXPECT_EQ(trades[2].quantity, 2);
    EXPECT_EQ(trades[2].restingTraderID, 1);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 1010000);
    EXPECT_EQ(bestAsk->quantity, 3);
}

TEST_F(MatchingEngineTestV2, StopMarketOrder_TriggersAndFills) {
    // Resting sell order
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "105.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);

    // Buy stop order, should trigger when price goes >= 100.00
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::STOP_MARKET, .side = Side::BUY, .stopPrice = "100.00", .quantity = 10, .traderID = 2});

    // A trade that should not trigger the stop
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "99.00", .quantity = 1, .traderID = 3});
    listener.waitForEvents(0, 0, 2, 0);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "99.00", .quantity = 1, .traderID = 4});
    listener.waitForEvents(1, 0, 2, 0);
    EXPECT_EQ(listener.getTrades().size(), 1);

    // This trade should trigger the stop order
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 5});
    listener.waitForEvents(1, 0, 3, 0);
    OrderID aggressingID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 5, .traderID = 6});
    
    listener.waitForEvents(3, 0, 3, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 3);

    // The third trade should be the triggered stop order filling against the resting order
    EXPECT_EQ(trades[2].price, 1050000);
    EXPECT_EQ(trades[2].quantity, 10);
}

TEST_F(MatchingEngineTestV2, StopLimitOrder_TriggersAndRests) {
    // Buy stop limit order, should trigger when price goes >= 100.00 and then rest as a limit order at 101.00
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::STOP_LIMIT, .side = Side::BUY, .price = "101.00", .stopPrice = "100.00", .quantity = 10, .traderID = 1});

    // This trade should trigger the stop order
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 2});
    listener.waitForEvents(0, 0, 1, 0);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 5, .traderID = 3});
    
    listener.waitForEvents(1, 0, 2, 0);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1010000);
    EXPECT_EQ(bestBid->quantity, 10);
}

TEST_F(MatchingEngineTestV2, LimitOrderIOC_PartialFill_CancelsRemainder) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);
    OrderID iocOrderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 2, .timeInForce = TimeInForce::IOC});

    listener.waitForEvents(1, 1, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].orderID, iocOrderID);
    EXPECT_EQ(cancellations[0].quantity, 5);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isEmpty());
}

TEST_F(MatchingEngineTestV2, LimitOrderFOK_FullFill_Executes) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 2, .timeInForce = TimeInForce::FOK});

    listener.waitForEvents(1, 0, 1, 0);

    auto trades = listener.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    EXPECT_TRUE(book->isEmpty());
}

TEST_F(MatchingEngineTestV2, LimitOrderFOK_PartialFill_Cancels) {
    engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 5, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);
    OrderID fokOrderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 2, .timeInForce = TimeInForce::FOK});

    listener.waitForEvents(0, 1, 1, 0);

    EXPECT_TRUE(listener.getTrades().empty());

    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].orderID, fokOrderID);
    EXPECT_EQ(cancellations[0].quantity, 10);

    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestAsk = book->getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 1000000);
    EXPECT_EQ(bestAsk->quantity, 5);
}

TEST_F(MatchingEngineTestV2, SelfMatchPrevention_RestingOrderCancelled) {
    // Trader 1 places a SELL limit order
    OrderID restingSellOrderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::SELL, .price = "100.00", .quantity = 10, .traderID = 1});
    listener.waitForEvents(0, 0, 1, 0);
    listener.clear(); // Clear events to only capture new events

    // Trader 1 places a BUY limit order at the same price (should trigger self-match prevention)
    OrderID incomingBuyOrderID = engine.submitOrder({.symbol = "AAPL", .orderType = OrderType::LIMIT, .side = Side::BUY, .price = "100.00", .quantity = 10, .traderID = 1});
    
    // Expect 1 cancellation (resting sell order) and 1 acceptance (incoming buy order)
    listener.waitForEvents(0, 1, 1, 0); 

    // Verify no trades occurred
    EXPECT_TRUE(listener.getTrades().empty());

    // Verify the resting sell order was cancelled
    auto cancellations = listener.getCancellations();
    ASSERT_EQ(cancellations.size(), 1);
    EXPECT_EQ(cancellations[0].orderID, restingSellOrderID);
    EXPECT_EQ(cancellations[0].traderID, 1);
    EXPECT_EQ(cancellations[0].quantity, 10);

    // Verify the incoming buy order was accepted
    auto acceptances = listener.getAcceptances();
    ASSERT_EQ(acceptances.size(), 1);
    EXPECT_EQ(acceptances[0].orderID, incomingBuyOrderID);
    EXPECT_EQ(acceptances[0].traderID, 1);

    // Verify the incoming buy order is now resting on the book
    OrderBook* book = engine.getBook(aapl_id);
    ASSERT_NE(book, nullptr);
    auto bestBid = book->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 1000000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_TRUE(book->isSideEmpty(Side::SELL)); // The sell side should be empty as the resting order was cancelled
}
