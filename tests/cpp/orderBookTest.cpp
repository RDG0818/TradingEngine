#include "gtest/gtest.h"
#include "trading_engine/orderBook.h"
#include "trading_engine/order.h"
#include "trading_engine/limitOrder.h"
#include "trading_engine/types.h"
#include "trading_engine/symbolRegistry.h"
#include <memory> // Required for std::unique_ptr
#include <optional> // Required for std::optional

// Test fixture for the OrderBook class
class OrderBookTest : public ::testing::Test {
protected:
    std::unique_ptr<OrderBook> ob;
    SymbolID aapl_id;

    void SetUp() override {
        ob = std::make_unique<OrderBook>();
        aapl_id = SymbolRegistry::getInstance().getID("AAPL");
    }
};

// Test that an empty book correctly returns no best bid or ask
TEST_F(OrderBookTest, GetBestBidAndAsk_ReturnsNulloptOnEmptyBook) {
    EXPECT_FALSE(ob->getBestBid().has_value());
    EXPECT_FALSE(ob->getBestAsk().has_value());
}

// Test adding a single buy order and verifying the best bid
TEST_F(OrderBookTest, AddSingleBuyOrder_CorrectlySetsBestBid) {
    auto buyOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
    ob->addOrder(std::move(buyOrder));
    
    auto bestBid = ob->getBestBid();

    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_FALSE(ob->getBestAsk().has_value());
}

// Test that orders at the same price level have their quantities aggregated
TEST_F(OrderBookTest, AddMultipleOrdersAtSamePrice_AggregatesQuantity) {
    auto buyOrder1 = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 2);
    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 15);
}

// Test adding orders at different price levels to verify correct best bid/ask
TEST_F(OrderBookTest, AddDifferentPriceOrders_CorrectlyIdentifiesBestBidAndAsk) {
    ob->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    ob->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));
    ob->addOrder(std::make_unique<LimitOrder>(aapl_id, 3, Side::SELL, 11000, 8, 2));
    ob->addOrder(std::make_unique<LimitOrder>(aapl_id, 4, Side::SELL, 10800, 12, 2));

    auto bestBid = ob->getBestBid();
    auto bestAsk = ob->getBestAsk();

    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10500);
    EXPECT_EQ(bestBid->quantity, 5);

    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 10800);
    EXPECT_EQ(bestAsk->quantity, 12);
}

// Test that adding an order with a duplicate ID throws an exception
TEST_F(OrderBookTest, AddDuplicateOrderID_ThrowsException) {
    auto firstOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 1, 1);
    auto secondOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::SELL, 20000, 2, 2);

    ob->addOrder(std::move(firstOrder));

    ASSERT_THROW(ob->addOrder(std::move(secondOrder)), std::invalid_argument);
}

// Test that removing a non-existent order throws an exception
TEST_F(OrderBookTest, RemoveNonExistentOrder_ThrowsException) {
    ASSERT_THROW(ob->removeOrder(999), std::invalid_argument);
}

// Test removing an existing order and verifying the book is updated
TEST_F(OrderBookTest, RemoveExistingOrder_UpdatesBookCorrectly) {
    auto buyOrder1 = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1);

    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    ob->removeOrder(2);

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    
    ob->removeOrder(1);
    EXPECT_FALSE(ob->getBestBid().has_value());
}

TEST_F(OrderBookTest, ReduceOrderQuantity_PartialFill) {
    auto buyOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 15, 1);
    ob->addOrder(std::move(buyOrder));

    ob->reduceOrderQuantity(1, 5);

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 10);

    Order* order = ob->getOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getQuantity(), 10);
}

TEST_F(OrderBookTest, ReduceOrderQuantity_FullFill) {
    auto buyOrder1 = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 1);
    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    ob->reduceOrderQuantity(1, 10);

    EXPECT_EQ(ob->getOrder(1), nullptr);

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 5);
}