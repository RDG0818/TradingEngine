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

class TestEventListener : public EventListener {
public:
    void onEvent(const Event& event) override {
        std::lock_guard<std::mutex> lock(mtx);
        if (event.type == EventType::TRADE_EXECUTED) {
            trades.push_back(static_cast<const TradeExecutedEvent&>(event));
        } else if (event.type == EventType::ORDER_CANCELLED) {
            cancellations.push_back(static_cast<const OrderCancelledEvent&>(event));
        } else if (event.type == EventType::ORDER_ACCEPTED) {
            acceptances.push_back(static_cast<const OrderAcceptedEvent&>(event));
        } else if (event.type == EventType::ORDER_REJECTED) {
            rejections.push_back(static_cast<const OrderRejectedEvent&>(event));
        }
        cv.notify_one();
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
    TestEventListener listener;
    SymbolID aapl_id;
    SymbolID goog_id;

    MatchingEngineTestV2() : engine(dispatcher) {}

    void SetUp() override {
        dispatcher.subscribe(EventType::TRADE_EXECUTED, &listener);
        dispatcher.subscribe(EventType::ORDER_CANCELLED, &listener);
        dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);
        dispatcher.subscribe(EventType::ORDER_REJECTED, &listener);
        engine.start();
        aapl_id = SymbolRegistry::getInstance().registerSymbol("AAPL");
        goog_id = SymbolRegistry::getInstance().registerSymbol("GOOG");
    }

    void TearDown() override {
        engine.stop();
    }
};

TEST_F(MatchingEngineTestV2, SubmitLimitOrder_NoMatch_RestsOnBook) {
    RawOrderParams params = {"AAPL", 1, Side::BUY, OrderType::LIMIT, 10, 1000000};
    OrderID orderID = engine.submitOrder(params);
    
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
