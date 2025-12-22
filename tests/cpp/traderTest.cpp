// tests/cpp/traderTest.cpp

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "eventDispatcher.h"
#include "matchingEngine.h"
#include "symbolRegistry.h"
#include "trader.h"

class MockMatchingEngine : public MatchingEngine {

public:

  using MatchingEngine::MatchingEngine;

  OrderID submit_order(const RawOrderParams& params) override {
    submitted_orders.push_back(params);
    return 1; // Return a dummy OrderID
  }

  std::optional<MarketData> get_best_bid(SymbolID symbol_id) const override {
    return mock_best_bid;
  }

  std::optional<MarketData> get_best_ask(SymbolID symbol_id) const override {
    return mock_best_ask;
  }

  void set_mock_bbo(std::optional<MarketData> bid, std::optional<MarketData> ask) {
    mock_best_bid = bid;
    mock_best_ask = ask;
  }

  std::vector<RawOrderParams> submitted_orders;
  std::optional<MarketData> mock_best_bid;
  std::optional<MarketData> mock_best_ask;
};

class TraderTest : public ::testing::Test {

protected:

  EventDispatcher dispatcher;
  MockMatchingEngine mock_engine;
  
  std::vector<std::string> symbols_ = {"AAPL", "GOOG"};
  SymbolID aapl_id_ = SymbolRegistry::get_instance().get_id("AAPL");
  SymbolID goog_id_ = SymbolRegistry::get_instance().get_id("GOOG");

  TraderTest() : mock_engine(dispatcher) {}
};

// --- Test Cases ---

TEST_F(TraderTest, RandomMarketTrader_SubmitsMarketOrder) {
  float lambda = 10.0; 
  auto time_delta = std::chrono::seconds(1);
  RandomMarketTrader trader(mock_engine, dispatcher, 1, lambda, time_delta, symbols_, 5);

  trader.tick();

  ASSERT_GE(mock_engine.submitted_orders.size(), 1);
  const auto& order = mock_engine.submitted_orders.front();
  EXPECT_EQ(order.trader_id, 1);
  EXPECT_EQ(order.order_type, OrderType::MARKET);
}

TEST_F(TraderTest, RandomLimitTrader_SubmitsLimitOrder) {
  float lambda = 10.0;
  auto time_delta = std::chrono::seconds(1);
  RandomLimitTrader trader(mock_engine, dispatcher, 2, lambda, time_delta, symbols_, 10, 0.05);

  // Set a mock book where the best bid is 100.00
  mock_engine.set_mock_bbo(MarketData{1000000, 100}, MarketData{1010000, 100});

  trader.tick();

  ASSERT_GE(mock_engine.submitted_orders.size(), 1);
  const auto& order = mock_engine.submitted_orders.front();
  EXPECT_EQ(order.trader_id, 2);
  EXPECT_EQ(order.order_type, OrderType::LIMIT);
  EXPECT_FALSE(order.price.empty()); // Price should not be empty for a limit order.
}

TEST_F(TraderTest, MarketMakerTrader_QuotesBidAndAsk) {
  auto time_delta = std::chrono::milliseconds(100);
  std::unordered_map<std::string, double> initial_prices = {{"AAPL", 150.0}};
  MarketMakerTrader trader(mock_engine, dispatcher, 3, {"AAPL"}, 0.0, 0.01, 0.01, time_delta, 10, initial_prices);

  trader.tick();

  ASSERT_EQ(mock_engine.submitted_orders.size(), 2);
  const auto& order1 = mock_engine.submitted_orders[0];
  const auto& order2 = mock_engine.submitted_orders[1];

  EXPECT_EQ(order1.trader_id, 3);
  EXPECT_EQ(order2.trader_id, 3);

  EXPECT_TRUE((order1.side == Side::BUY && order2.side == Side::SELL) || (order1.side == Side::SELL && order2.side == Side::BUY));
    
  // Prices should be based on the fair price (150.0) and spread (0.01)
  // Bid should be ~150 * (1 - 0.01) = 148.5
  // Ask should be ~150 * (1 + 0.01) = 151.5
  Price expected_bid = 1485000;
  Price expected_ask = 1515000;

  Price actual_bid = (order1.side == Side::BUY) ? std::stoull(order1.price) * 10000 : std::stoull(order2.price) * 10000;
  Price actual_ask = (order1.side == Side::SELL) ? std::stoull(order1.price) * 10000 : std::stoull(order2.price) * 10000;

  EXPECT_NEAR(actual_bid, expected_bid, 5000); // Allow 0.5 price deviation
  EXPECT_NEAR(actual_ask, expected_ask, 5000); // Allow 0.5 price deviation
}

TEST_F(TraderTest, MarketMakerTrader_AdjustsToMarket) {
  auto time_delta = std::chrono::milliseconds(100);
  std::unordered_map<std::string, double> initial_prices = {{"AAPL", 150.0}};
  MarketMakerTrader trader(mock_engine, dispatcher, 3, {"AAPL"}, 0.0, 0.01, 0.01, time_delta, 10, initial_prices);

  mock_engine.set_mock_bbo(MarketData{1990000, 100}, MarketData{2010000, 100});

  trader.tick();

  ASSERT_EQ(mock_engine.submitted_orders.size(), 2);
  const auto& order1 = mock_engine.submitted_orders[0];
  const auto& order2 = mock_engine.submitted_orders[1];

  // New fair price is (199+201)/2 = 200.
  // Bid should be ~200 * (1 - 0.01) = 198
  // Ask should be ~200 * (1 + 0.01) = 202
  Price expected_bid = 1980000;
  Price expected_ask = 2020000;
  
  Price actual_bid = (order1.side == Side::BUY) ? std::stoull(order1.price) * 10000 : std::stoull(order2.price) * 10000;
  Price actual_ask = (order1.side == Side::SELL) ? std::stoull(order1.price) * 10000 : std::stoull(order2.price) * 10000;

  // Allow for small deviation
  EXPECT_NEAR(actual_bid, expected_bid, 5000);
  EXPECT_NEAR(actual_ask, expected_ask, 5000);
}

