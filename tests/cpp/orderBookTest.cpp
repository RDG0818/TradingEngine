#include "gtest/gtest.h"
#include "orderBook.h"
#include "order.h"
#include "utils.h"
#include "symbolRegistry.h"
#include <memory>
#include <optional>

class OrderBookTest : public ::testing::Test {
protected:
    std::unique_ptr<OrderBook> orderBook;
    SymbolID aapl_id;

    void SetUp() override {
        orderBook = std::make_unique<OrderBook>();
        aapl_id = SymbolRegistry::get_instance().get_id("AAPL");
    }
};

// Test that an empty book correctly returns no best bid or ask.
TEST_F(OrderBookTest, IsEmptyBookInitially) {
    EXPECT_FALSE(orderBook->getBestBid().has_value());
    EXPECT_FALSE(orderBook->getBestAsk().has_value());
    EXPECT_TRUE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_TRUE(orderBook->isSideEmpty(Side::SELL));
}

// Test adding a single buy order and verifying the best bid.
TEST_F(OrderBookTest, AddSingleBuyOrderShouldUpdateBestBid) {
    auto buyOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
    orderBook->addOrder(std::move(buyOrder));
    
    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    EXPECT_FALSE(orderBook->getBestAsk().has_value());
}

// Test that orders at the same price level have their quantities aggregated.
TEST_F(OrderBookTest, AddMultipleOrdersAtSamePriceShouldAggregateQuantity) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 2));

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 15);
}

// Test adding orders at different price levels to verify correct best bid/ask.
TEST_F(OrderBookTest, AddOrdersAtDifferentPricesShouldSetCorrectBestBidAndAsk) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 3, Side::SELL, 11000, 8, 2));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 4, Side::SELL, 10800, 12, 2));

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10500);
    EXPECT_EQ(bestBid->quantity, 5);

    auto bestAsk = orderBook->getBestAsk();
    ASSERT_TRUE(bestAsk.has_value());
    EXPECT_EQ(bestAsk->price, 10800);
    EXPECT_EQ(bestAsk->quantity, 12);
}

// Test that adding an order with a duplicate ID throws an exception.
TEST_F(OrderBookTest, AddOrderWithDuplicateIdShouldThrow) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 1, 1));
    ASSERT_THROW(orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::SELL, 20000, 2, 2)), std::invalid_argument);
}

TEST_F(OrderBookTest, CancelNonExistentOrderShouldNotThrow) {
    ASSERT_NO_THROW(orderBook->cancelOrder(999));
}

// Test removing an existing order and verifying the book is updated.
TEST_F(OrderBookTest, RemoveExistingOrderShouldUpdateBook) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));

    orderBook->cancelOrder(2);

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    
    orderBook->cancelOrder(1);
    EXPECT_FALSE(orderBook->getBestBid().has_value());
}

// Test reducing an order's quantity with a partial fill.
TEST_F(OrderBookTest, ReduceOrderQuantityForPartialFill) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 15, 1));

    orderBook->reduceOrderQuantity(1, 5);

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 10);

    Order* order = orderBook->getOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->get_quantity(), 10);
}

// Test reducing an order's quantity with a full fill, which should remove the order.
TEST_F(OrderBookTest, ReduceOrderQuantityForFullFill) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 1));

    orderBook->reduceOrderQuantity(1, 10);

    EXPECT_EQ(orderBook->getOrder(1), nullptr);

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->quantity, 5);
}

// Test cancelling an order and ensuring it's removed from the book.
TEST_F(OrderBookTest, CancelOrderShouldRemoveFromBook) {
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));

    orderBook->cancelOrder(2);

    auto bestBid = orderBook->getBestBid();
    ASSERT_TRUE(bestBid.has_value());
    EXPECT_EQ(bestBid->price, 10000);
    EXPECT_EQ(bestBid->quantity, 10);
    
    orderBook->cancelOrder(1);
    EXPECT_FALSE(orderBook->getBestBid().has_value());
}

// Test checking if a side of the book is empty.
TEST_F(OrderBookTest, IsSideEmptyShouldReturnCorrectStatus) {
    EXPECT_TRUE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_TRUE(orderBook->isSideEmpty(Side::SELL));

    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
    EXPECT_FALSE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_TRUE(orderBook->isSideEmpty(Side::SELL));

    orderBook->addOrder(std::make_unique<LimitOrder>(aapl_id, 2, Side::SELL, 11000, 5, 2));
    EXPECT_FALSE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_FALSE(orderBook->isSideEmpty(Side::SELL));

    orderBook->cancelOrder(1);
    EXPECT_TRUE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_FALSE(orderBook->isSideEmpty(Side::SELL));

    orderBook->cancelOrder(2);
    EXPECT_TRUE(orderBook->isSideEmpty(Side::BUY));
    EXPECT_TRUE(orderBook->isSideEmpty(Side::SELL));
}