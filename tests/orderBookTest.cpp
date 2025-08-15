#include "orderBook.h"
#include "gtest/gtest.h"

TEST(OrderBookTest, AddBuyOrderCorrectly) {
    // Arrange
    OrderBook ob;
    Order buyOrder(1, Side::BUY, 100, 1, 1, 1);

    // Act
    ob.addOrder(buyOrder);
    MarketData bestBid = ob.getBid();

    // Assert
    EXPECT_EQ(bestBid.price, 100);
    EXPECT_EQ(bestBid.quantity, 1);
}