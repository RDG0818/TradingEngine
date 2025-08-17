#include "gtest/gtest.h"
#include "orderBook.h"
#include "order.h"
#include "limitOrder.h"
#include "types.h"
#include <memory> // Required for std::unique_ptr
#include <optional> // Required for std::optional

// Test fixture for the OrderBook class
class OrderBookTest : public ::testing::Test {
protected:
    // Use a smart pointer for automatic memory management
    std::unique_ptr<OrderBook> ob;

    void SetUp() override {
        ob = std::make_unique<OrderBook>();
    }

    // No need for TearDown(), std::unique_ptr handles it
};

// Test that an empty book correctly returns no best bid or ask
TEST_F(OrderBookTest, GetBestBidAndAsk_ReturnsNulloptOnEmptyBook) {
    EXPECT_FALSE(ob->getBestBid().has_value());
    EXPECT_FALSE(ob->getBestAsk().has_value());
}

// Test adding a single buy order and verifying the best bid
TEST_F(OrderBookTest, AddSingleBuyOrder_CorrectlySetsBestBid) {
    auto buyOrder = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 10, 1);

    ob->addOrder(std::move(buyOrder));
    
    auto bestBid = ob->getBestBid();

    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_FALSE(ob->getBestAsk().has_value()); // Ensure ask side is still empty
}

// Test that orders at the same price level have their quantities aggregated
TEST_F(OrderBookTest, AddMultipleOrdersAtSamePrice_AggregatesQuantity) {
    auto buyOrder1 = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(2, OrderType::LIMIT, Side::BUY, "100.00", 5, 2);

    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 15);
}

// Test adding orders at different price levels to verify correct best bid/ask
TEST_F(OrderBookTest, AddDifferentPriceOrders_CorrectlyIdentifiesBestBidAndAsk) {
    auto buyOrder1 = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 10, 1); // Lower bid
    auto buyOrder2 = std::make_unique<LimitOrder>(2, OrderType::LIMIT, Side::BUY, "105.00", 5, 1); // Best bid
    
    auto sellOrder1 = std::make_unique<LimitOrder>(3, OrderType::LIMIT, Side::SELL, "110.00", 8, 2); // Higher ask
    auto sellOrder2 = std::make_unique<LimitOrder>(4, OrderType::LIMIT, Side::SELL, "108.00", 12, 2); // Best ask

    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));
    ob->addOrder(std::move(sellOrder1));
    ob->addOrder(std::move(sellOrder2));

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
    auto firstOrder = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 1, 1);
    auto secondOrder = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::SELL, "200.00", 2, 2); // Same ID

    ob->addOrder(std::move(firstOrder));

    ASSERT_THROW(ob->addOrder(std::move(secondOrder)), std::invalid_argument);
}

// Test that removing a non-existent order throws an exception
TEST_F(OrderBookTest, RemoveNonExistentOrder_ThrowsException) {
    ASSERT_THROW(ob->removeOrder(999), std::invalid_argument);
}

// Test removing an existing order and verifying the book is updated
TEST_F(OrderBookTest, RemoveExistingOrder_UpdatesBookCorrectly) {
    auto buyOrder1 = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(2, OrderType::LIMIT, Side::BUY, "105.00", 5, 1); // Best bid

    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    ob->removeOrder(2); // Remove the best bid

    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000); // Best bid should now be the other order
    EXPECT_EQ(bestBid->quantity, 10);
    
    ob->removeOrder(1); // Remove the last order
    EXPECT_FALSE(ob->getBestBid().has_value()); // Book should be empty
}

TEST_F(OrderBookTest, ReduceOrderQuantity_PartialFill) {
    auto buyOrder = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 15, 1);
    ob->addOrder(std::move(buyOrder));

    // Reduce the order's quantity by 5 (a partial fill)
    ob->reduceOrderQuantity(1, 5);

    // Check the aggregated quantity at the price level
    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 10);

    // Check the individual order's quantity
    Order* order = ob->getOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getQuantity(), 10);
}

TEST_F(OrderBookTest, ReduceOrderQuantity_FullFill) {
    auto buyOrder1 = std::make_unique<LimitOrder>(1, OrderType::LIMIT, Side::BUY, "100.00", 10, 1);
    auto buyOrder2 = std::make_unique<LimitOrder>(2, OrderType::LIMIT, Side::BUY, "100.00", 5, 1);
    ob->addOrder(std::move(buyOrder1));
    ob->addOrder(std::move(buyOrder2));

    // Fully fill the first order
    ob->reduceOrderQuantity(1, 10);

    // Check that the first order is completely gone
    EXPECT_EQ(ob->getOrder(1), nullptr);

    // Check that the aggregated quantity now only reflects the second order
    auto bestBid = ob->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 5);
}