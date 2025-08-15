#include "orderBook.h"
#include "gtest/gtest.h"

class OrderBookTest : public ::testing::Test {
    protected:
        void SetUp() override {
            ob = new OrderBook();
        }
        void TearDown() override {
            delete ob;
        }

        OrderBook* ob;
};

TEST_F(OrderBookTest, AddSingleBuyOrder_CorrectlySetsBestBid) {
    Order buyOrder(1, Side::BUY, "100.00", 1, 1, 1);

    ob->addOrder(buyOrder);
    MarketData bestBid = ob->getBid();

    EXPECT_EQ(bestBid.price, 10000);
    EXPECT_EQ(bestBid.quantity, 1);
}

TEST_F(OrderBookTest, AddMultipleOrders_AggregatesQuantity) {
    Order buyOrder1(1, Side::BUY, "100.00", 10, 1, 1);
    Order buyOrder2(2, Side::BUY, "100.00", 5, 2, 1);

    ob->addOrder(buyOrder1);
    ob->addOrder(buyOrder2);

    MarketData bid = ob->getBid();
    EXPECT_EQ(bid.price, 10000);
    EXPECT_EQ(bid.quantity, 15);
}

TEST_F(OrderBookTest, AddDiffPriceOrders_BestBidAsk) {
    Order buyOrder1(1, Side::BUY, "100.00", 10, 1, 1);
    Order buyOrder2(2, Side::BUY, "105.15", 5, 2, 1); // Best Bid
    Order sellOrder1(3, Side::SELL, "110.10", 8, 3, 1);
    Order sellOrder2(4, Side::SELL, "108.65", 12, 4, 1); // Best Ask

    ob->addOrder(buyOrder1);
    ob->addOrder(buyOrder2);
    ob->addOrder(sellOrder1);
    ob->addOrder(sellOrder2);

    MarketData bestBid = ob->getBid();
    MarketData bestAsk = ob->getAsk();

    EXPECT_EQ(bestBid.price, 10515);
    EXPECT_EQ(bestBid.quantity, 5);
    EXPECT_EQ(bestAsk.price, 10865);
    EXPECT_EQ(bestAsk.quantity, 12);
}

TEST_F(OrderBookTest, AddOrderWithZeroQuantity_ThrowsException) {
    ASSERT_THROW({
        Order invalidOrder(1, Side::BUY, "100.25", 0, 1, 1);
    }, std::invalid_argument);
}

TEST_F(OrderBookTest, AddDuplicateOrder_ThrowsException) {
    Order firstOrder(1, Side::BUY, "100.00", 1, 1, 1);
    Order secondOrder(1, Side::BUY, "200.00", 2, 3, 4);
    ob->addOrder(firstOrder);
    ASSERT_THROW({
        ob->addOrder(secondOrder);
    }, std::invalid_argument);
}

TEST_F(OrderBookTest, CancelNonExistentOrder_ThrowsException) {
    Order nonExistentOrder(999, Side::BUY, "100.00", 1, 1, 1);
    
    ASSERT_THROW(ob->cancelOrder(nonExistentOrder), std::invalid_argument);
}

TEST_F(OrderBookTest, CancelExistingBuyOrder_RemovesOrderCorrectly) {
    Order buyOrder(1, Side::BUY, "100.00", 10, 1, 1);
    Order sellOrder(2, Side::SELL, "200.00", 1, 2, 3);
    ob->addOrder(buyOrder);
    ob->addOrder(sellOrder);
    ob->cancelOrder(buyOrder);
    ob->cancelOrder(sellOrder);
    ASSERT_THROW({ob->getBid();}, std::invalid_argument);
    ASSERT_THROW({ob->getAsk();}, std::invalid_argument);
}
