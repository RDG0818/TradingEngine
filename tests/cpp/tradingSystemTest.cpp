// tests/cpp/tradingSystemTest.cpp

#include "gtest/gtest.h"
#include "tradingSystem.h"
#include "events.h"
#include "order.h"
#include "symbolRegistry.h"
#include "utils.h"
#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

class TradingSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<TradingSystem> trading_system;
    const std::vector<std::string> symbols = {"AAPL", "GOOG"};
    SymbolID aapl_id;
    SymbolID goog_id;

    void SetUp() override {
        // Using a small tick interval for faster tests
        trading_system = std::make_unique<TradingSystem>(1, symbols);
        trading_system->start();
        trading_system->enable_automated_traders(false); // Disable random traders for predictable tests
        aapl_id = SymbolRegistry::get_instance().get_id("AAPL");
        goog_id = SymbolRegistry::get_instance().get_id("GOOG");
    }

    void TearDown() override {
        if (trading_system) {
            trading_system->stop();
        }
    }


    void waitForProcessing() {
        // Give the system a moment to process asynchronous events
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

TEST_F(TradingSystemTest, LifecycleAndConfiguration) {
    // State from SetUp: running, traders disabled.
    EXPECT_TRUE(trading_system->is_running());
    EXPECT_FALSE(trading_system->are_automated_traders_enabled());

    // Test stop
    trading_system->stop();
    EXPECT_FALSE(trading_system->is_running());
    EXPECT_FALSE(trading_system->are_automated_traders_enabled()); // manager should also be stopped

    // Test start
    trading_system->start();
    EXPECT_TRUE(trading_system->is_running());
    EXPECT_TRUE(trading_system->are_automated_traders_enabled()); // start() enables traders by default

    // Test explicitly enabling traders (should be idempotent)
    trading_system->enable_automated_traders(true);
    EXPECT_TRUE(trading_system->are_automated_traders_enabled());

    // Test disabling traders
    trading_system->enable_automated_traders(false);
    EXPECT_FALSE(trading_system->are_automated_traders_enabled());

    const auto& all_symbols = trading_system->get_all_symbols();
    ASSERT_EQ(all_symbols.size(), 2);
    EXPECT_EQ(all_symbols[0], "AAPL");
    EXPECT_EQ(all_symbols[1], "GOOG");
}

TEST_F(TradingSystemTest, PortfolioManagement) {
    TraderID trader_id = trading_system->create_portfolio(100000000); // $10,000.00
    EXPECT_GT(trader_id, 0);

    waitForProcessing();

    auto snapshot_opt = trading_system->get_portfolio_snapshot(trader_id);
    ASSERT_TRUE(snapshot_opt.has_value());
    auto snapshot = snapshot_opt.value();

    EXPECT_EQ(snapshot.balance, 100000000);
    EXPECT_TRUE(snapshot.positions.empty());
    EXPECT_TRUE(snapshot.trade_history.empty());

    auto non_existent_snapshot = trading_system->get_portfolio_snapshot(99999);
    EXPECT_FALSE(non_existent_snapshot.has_value());
}

TEST_F(TradingSystemTest, SubmitAndCancelOrder) {
    TraderID trader_id = trading_system->create_portfolio(200000000);
    
    // Submit
    OrderID order_id = trading_system->submit_order(trader_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "150.00", .quantity = 10, .trader_id = trader_id});
    
    waitForProcessing();

    auto metrics = trading_system->get_system_metrics();
    EXPECT_EQ(metrics.active_orders, 1);

    auto market_snapshot = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot.has_value());
    EXPECT_EQ(market_snapshot->best_bid, 1500000);
    EXPECT_EQ(market_snapshot->best_bid_quantity, 10);

    // Cancel
    trading_system->cancel_order(order_id);
    waitForProcessing();

    metrics = trading_system->get_system_metrics();
    EXPECT_EQ(metrics.active_orders, 0);

    market_snapshot = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot.has_value());
    EXPECT_EQ(market_snapshot->best_bid, 0);
    EXPECT_EQ(market_snapshot->best_bid_quantity, 0);
}

TEST_F(TradingSystemTest, OrderRejection_InsufficientFunds) {
    TraderID trader_id = trading_system->create_portfolio(1000000); // $100.00

    // Try to buy 10 shares at $150 each ($1500 total)
    trading_system->submit_order(trader_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "150.00", .quantity = 10, .trader_id = trader_id});

    waitForProcessing();

    auto metrics = trading_system->get_system_metrics();
    EXPECT_EQ(metrics.active_orders, 0);

    auto portfolio_snapshot = trading_system->get_portfolio_snapshot(trader_id);
    ASSERT_TRUE(portfolio_snapshot.has_value());
    EXPECT_EQ(portfolio_snapshot->balance, 1000000); // Balance should be unchanged

    auto market_snapshot = trading_system->get_market_snapshot("AAPL");
    // Snapshot might not exist if no order was ever accepted for it.
    if (market_snapshot.has_value()) {
        EXPECT_EQ(market_snapshot->best_bid, 0);
    }
}

TEST_F(TradingSystemTest, OrderRejection_InvalidTrader) {
    TraderID invalid_trader_id = 999;
    EXPECT_THROW(
        trading_system->submit_order(invalid_trader_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "150.00", .quantity = 10, .trader_id = invalid_trader_id}),
        std::invalid_argument
    );
}

TEST_F(TradingSystemTest, TradeExecution_FullMatch) {
    Price starting_balance = 200000000; // $20,000.00
    Price trade_price = 1500000; // $150.00
    Quantity trade_quantity = 10;
    Price trade_cost = trade_price * trade_quantity;

    TraderID buyer_id = trading_system->create_portfolio(starting_balance);

    // System trader ID to bypass portfolio checks for the sell side
    TraderID seller_id = 100001; 

    // Buyer places an order
    OrderID buy_order_id = trading_system->submit_order(buyer_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "150.00", .quantity = trade_quantity, .trader_id = buyer_id});
    waitForProcessing();

    // Check state before match
    auto buyer_snapshot_before = trading_system->get_portfolio_snapshot(buyer_id);
    ASSERT_TRUE(buyer_snapshot_before.has_value());
    // NOTE: We assume the portfolio doesn't deduct balance until a trade is confirmed.
    EXPECT_EQ(buyer_snapshot_before->balance, starting_balance);

    auto metrics_before = trading_system->get_system_metrics();
    EXPECT_EQ(metrics_before.active_orders, 1);

    // Seller places matching order
    OrderID sell_order_id = trading_system->submit_order(seller_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "150.00", .quantity = trade_quantity, .trader_id = seller_id});
    waitForProcessing();

    // Check state after match
    auto metrics_after = trading_system->get_system_metrics();
    EXPECT_EQ(metrics_after.active_orders, 0);
    EXPECT_EQ(metrics_after.orders_processed, 1);

    auto market_snapshot = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot.has_value());
    EXPECT_EQ(market_snapshot->best_bid, 0);
    EXPECT_EQ(market_snapshot->best_ask, 0);
    EXPECT_EQ(market_snapshot->last_trade_price, trade_price);
    EXPECT_EQ(market_snapshot->last_trade_quantity, trade_quantity);

    auto buyer_snapshot_after = trading_system->get_portfolio_snapshot(buyer_id);
    ASSERT_TRUE(buyer_snapshot_after.has_value());
    EXPECT_EQ(buyer_snapshot_after->balance, starting_balance - trade_cost);
    ASSERT_EQ(buyer_snapshot_after->positions.count(aapl_id), 1);
    EXPECT_EQ(buyer_snapshot_after->positions.at(aapl_id), trade_quantity);
    EXPECT_EQ(buyer_snapshot_after->trade_history.size(), 1);
    EXPECT_EQ(buyer_snapshot_after->trade_history.front().price, trade_price);
}

TEST_F(TradingSystemTest, SystemMetricsAndMarketSnapshotUpdate) {
    Price starting_balance = 200000000; // $20,000.00
    TraderID buyer_id = trading_system->create_portfolio(starting_balance);
    TraderID seller_id = 100001; // System/automated trader ID to bypass portfolio checks

    // 1. Test Latency Metric
    // Submit an order and check if latency is recorded.
    trading_system->submit_order(buyer_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "149.00", .quantity = 10, .trader_id = buyer_id});
    waitForProcessing();

    auto metrics = trading_system->get_system_metrics();
    // The latency will be very small, but should be a positive value.
    EXPECT_GT(metrics.avg_latency_ms, 0.0);
    // It should be a very small number, testing for a reasonable upper bound
    EXPECT_LT(metrics.avg_latency_ms, 100.0); // Expect latency < 100ms for a local system

    // 2. Test Recent Trades
    // The buy order for 10 shares at 149.00 is on the book.
    // Submit a matching sell order to trigger a trade.
    trading_system->submit_order(seller_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "149.00", .quantity = 5, .trader_id = seller_id});
    waitForProcessing();

    auto market_snapshot_1 = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot_1.has_value());
    ASSERT_EQ(market_snapshot_1->recent_trades.size(), 1);
    EXPECT_EQ(market_snapshot_1->recent_trades.front(), 1490000); // $149.00
    EXPECT_EQ(market_snapshot_1->last_trade_price, 1490000);

    // Trigger another trade to fill the rest of the original buy order
    trading_system->submit_order(seller_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "149.00", .quantity = 5, .trader_id = seller_id});
    waitForProcessing(); 

    auto market_snapshot_2 = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot_2.has_value());
    ASSERT_EQ(market_snapshot_2->recent_trades.size(), 2);
    EXPECT_EQ(market_snapshot_2->recent_trades[0], 1490000); 
    EXPECT_EQ(market_snapshot_2->recent_trades[1], 1490000);

    // Now place a new set of trades at a different price
    trading_system->submit_order(buyer_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::BUY, .price = "151.00", .quantity = 8, .trader_id = buyer_id});
    waitForProcessing();
    trading_system->submit_order(seller_id, {.symbol = "AAPL", .order_type = OrderType::LIMIT, .side = Side::SELL, .price = "151.00", .quantity = 8, .trader_id = seller_id});
    waitForProcessing();

    auto market_snapshot_3 = trading_system->get_market_snapshot("AAPL");
    ASSERT_TRUE(market_snapshot_3.has_value());
    ASSERT_EQ(market_snapshot_3->recent_trades.size(), 3);
    // Trades are added to the front (LIFO)
    EXPECT_EQ(market_snapshot_3->recent_trades[0], 1510000); // $151.00
    EXPECT_EQ(market_snapshot_3->recent_trades[1], 1490000);
    EXPECT_EQ(market_snapshot_3->recent_trades[2], 1490000);
    EXPECT_EQ(market_snapshot_3->last_trade_price, 1510000);
}
