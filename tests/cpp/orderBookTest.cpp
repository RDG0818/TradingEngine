// tests/cpp/orderBookTest.cpp

#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "orderBook.h"
#include "order.h"
#include "symbolRegistry.h"
#include "utils.h"

class OrderBookTest : public ::testing::Test {
protected:
  std::unique_ptr<OrderBook> orderBook;
  SymbolID aapl_id;

  void SetUp() override {
    orderBook = std::make_unique<OrderBook>();
    aapl_id = SymbolRegistry::get_instance().get_id("AAPL");
  }
};

// Test Cases 

TEST_F(OrderBookTest, IsEmptyBookInitially) {
  EXPECT_FALSE(orderBook->get_best_bid().has_value());
  EXPECT_FALSE(orderBook->get_best_ask().has_value());
  EXPECT_TRUE(orderBook->is_side_empty(Side::BUY));
  EXPECT_TRUE(orderBook->is_side_empty(Side::SELL));
}

TEST_F(OrderBookTest, AddSingleBuyOrderShouldUpdateBestBid) {
  auto buyOrder = std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1);
  orderBook->add_order(std::move(buyOrder));
    
  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->price, 10000);
  EXPECT_EQ(bestBid->quantity, 10);
  EXPECT_FALSE(orderBook->get_best_ask().has_value());
}

TEST_F(OrderBookTest, AddMultipleOrdersAtSamePriceShouldAggregateQuantity) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 2));

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->price, 10000);
  EXPECT_EQ(bestBid->quantity, 15);
}

TEST_F(OrderBookTest, add_ordersAtDifferentPricesShouldSetCorrectBestBidAndAsk) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 3, Side::SELL, 11000, 8, 2));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 4, Side::SELL, 10800, 12, 2));

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->price, 10500);
  EXPECT_EQ(bestBid->quantity, 5);

  auto bestAsk = orderBook->get_best_ask();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_EQ(bestAsk->price, 10800);
  EXPECT_EQ(bestAsk->quantity, 12);
}

TEST_F(OrderBookTest, add_orderWithDuplicateIdShouldThrow) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 1, 1));
  ASSERT_THROW(orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::SELL, 20000, 2, 2)), std::invalid_argument);
}

TEST_F(OrderBookTest, CancelNonExistentOrderShouldNotThrow) {
  ASSERT_NO_THROW(orderBook->cancel_order(999));
}

TEST_F(OrderBookTest, RemoveExistingOrderShouldUpdateBook) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));

  orderBook->cancel_order(2);

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->price, 10000);
  EXPECT_EQ(bestBid->quantity, 10);
    
  orderBook->cancel_order(1);
  EXPECT_FALSE(orderBook->get_best_bid().has_value());
}

TEST_F(OrderBookTest, ReduceOrderQuantityForPartialFill) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 15, 1));

  orderBook->reduce_order_quantity(1, 5);

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->quantity, 10);

  Order* order = orderBook->get_order(1);
  ASSERT_NE(order, nullptr);
  EXPECT_EQ(order->get_quantity(), 10);
}

TEST_F(OrderBookTest, reduce_order_quantityForFullFill) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10000, 5, 1));

  orderBook->reduce_order_quantity(1, 10);

  EXPECT_EQ(orderBook->get_order(1), nullptr);

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->quantity, 5);
}

TEST_F(OrderBookTest, cancel_orderShouldRemoveFromBook) {
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::BUY, 10500, 5, 1));

  orderBook->cancel_order(2);

  auto bestBid = orderBook->get_best_bid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->price, 10000);
  EXPECT_EQ(bestBid->quantity, 10);
  
  orderBook->cancel_order(1);
  EXPECT_FALSE(orderBook->get_best_bid().has_value());
}

TEST_F(OrderBookTest, is_side_emptyShouldReturnCorrectStatus) {
  EXPECT_TRUE(orderBook->is_side_empty(Side::BUY));
  EXPECT_TRUE(orderBook->is_side_empty(Side::SELL));

  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 1, Side::BUY, 10000, 10, 1));
  EXPECT_FALSE(orderBook->is_side_empty(Side::BUY));
  EXPECT_TRUE(orderBook->is_side_empty(Side::SELL));

  orderBook->add_order(std::make_unique<LimitOrder>(aapl_id, 2, Side::SELL, 11000, 5, 2));
  EXPECT_FALSE(orderBook->is_side_empty(Side::BUY));
  EXPECT_FALSE(orderBook->is_side_empty(Side::SELL));

  orderBook->cancel_order(1);
  EXPECT_TRUE(orderBook->is_side_empty(Side::BUY));
  EXPECT_FALSE(orderBook->is_side_empty(Side::SELL));

  orderBook->cancel_order(2);
  EXPECT_TRUE(orderBook->is_side_empty(Side::BUY));
  EXPECT_TRUE(orderBook->is_side_empty(Side::SELL));
}