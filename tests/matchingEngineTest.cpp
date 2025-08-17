#include "gtest/gtest.h"
#include "matchingEngine.h"
#include "orderBook.h"
#include "eventDispatcher.h"
#include "limitOrder.h"
#include "marketOrder.h"
#include "types.h"
#include <memory>
#include <vector>


class MockListener {
public:
    std::vector<TradeExecutedEvent> trades;
    std::vector<OrderCancelledEvent> cancellations;

    void subscribe(EventDispatcher& dispatcher) {
        dispatcher.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
            trades.push_back(event);
        });
        dispatcher.subscribe<OrderCancelledEvent>([this](const OrderCancelledEvent& event) {
            cancellations.push_back(event);
        });
    }
};

// --- Test Fixture for the MatchingEngine ---
class MatchingEngineTest : public ::testing::Test {
protected:
    // These are recreated for each test
    OrderBook book;
    EventDispatcher dispatcher;
    std::unique_ptr<MatchingEngine> engine;
    MockListener listener;

    void SetUp() override {
        engine = std::make_unique<MatchingEngine>(book, dispatcher);
        listener.subscribe(dispatcher);
    }
};


// --- TEST CASES ---

TEST_F(MatchingEngineTest, LimitOrder_NoMatch_RestsOnBook) {
    auto buyOrder = std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::BUY, "99.00", 10, 1);
    
    // Pre-load the book with a sell order that won't cross
    auto sellOrder = std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "101.00", 10, 2);
    engine->processOrder(std::move(sellOrder));

    // Process the buy order
    engine->processOrder(std::move(buyOrder));

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
    auto sellOrder = std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2);
    engine->processOrder(std::move(sellOrder));

    // Aggressing buy order for 15 shares at $100
    auto buyOrder = std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::BUY, "100.00", 15, 1);
    Order* buyOrderPtr = buyOrder.get();
    engine->processOrder(std::move(buyOrder));

    // VERIFY: One trade occurred for 10 shares
    ASSERT_EQ(listener.trades.size(), 1);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 10000);

    // VERIFY: The aggressing buy order has 5 shares remaining and is now on the book
    EXPECT_EQ(buyOrderPtr->getQuantity(), 5);
    EXPECT_EQ(buyOrderPtr->getOrderStatus(), OrderStatus::ACCEPTED);
    auto bestBid = book.getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 5);

    // VERIFY: The sell side of the book is now empty
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, LimitOrder_WalkTheBook_FullFill) {
    // Pre-load the book with multiple sell orders
    engine->processOrder(std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2));
    engine->processOrder(std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "101.00", 10, 3));

    // Aggressing buy order for 15 shares, willing to pay up to $101
    auto buyOrder = std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::BUY, "101.00", 15, 1);
    Order* buyOrderPtr = buyOrder.get();
    engine->processOrder(std::move(buyOrder));

    // VERIFY: Two trades occurred
    ASSERT_EQ(listener.trades.size(), 2);
    // Trade 1: 10 shares at $100
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[0].price, 10000);
    // Trade 2: 5 shares at $101
    EXPECT_EQ(listener.trades[1].quantity, 5);
    EXPECT_EQ(listener.trades[1].price, 10100);

    // VERIFY: Aggressing order is fully filled
    EXPECT_EQ(buyOrderPtr->getQuantity(), 0);
    EXPECT_EQ(buyOrderPtr->getOrderStatus(), OrderStatus::FILLED);

    // VERIFY: The book now has one sell order with 5 shares remaining
    auto bestAsk = book.getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 10100);
    EXPECT_EQ(bestAsk->quantity, 5);
}

TEST_F(MatchingEngineTest, MarketOrder_ClearsBook_RemainderCancelled) {
    // Pre-load the book with 15 shares total to sell
    engine->processOrder(std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "100.00", 10, 2));
    engine->processOrder(std::make_unique<LimitOrder>(0, OrderType::LIMIT, Side::SELL, "101.00", 5, 3));

    // Aggressing market order to buy 20 shares
    auto buyOrder = std::make_unique<MarketOrder>(0, OrderType::MARKET, Side::BUY, 20, 1);
    OrderID aggressingID = buyOrder->getOrderID();
    engine->processOrder(std::move(buyOrder));

    // VERIFY: Two trades occurred, clearing the book
    ASSERT_EQ(listener.trades.size(), 2);
    EXPECT_EQ(listener.trades[0].quantity, 10);
    EXPECT_EQ(listener.trades[1].quantity, 5);

    // VERIFY: The remaining 5 shares were cancelled
    ASSERT_EQ(listener.cancellations.size(), 1);
    EXPECT_EQ(listener.cancellations[0].orderID, 3);
    EXPECT_EQ(listener.cancellations[0].quantity, 5);

    // VERIFY: The sell side of the book is now empty
    EXPECT_FALSE(book.getBestAsk().has_value());
}
