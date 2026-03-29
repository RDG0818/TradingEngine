// tests/cpp/test_trader_registry.cpp
#include <gtest/gtest.h>
#include "include/trader_registry.h"
#include "include/order_matcher.h"
#include "include/event_bus.h"
#include <thread>
#include <chrono>

TEST(TraderRegistry, AddAndStartTrader) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher);

    auto id = registry.add_trader<RandomMarketTrader>("noise", 10000000ULL);
    registry.start_trader(id);
    registry.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    registry.stop();
    matcher.stop();

    auto info = registry.trader_info(id);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "noise");
}

TEST(TraderRegistry, RemoveTrader) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher);
    auto id = registry.add_trader<RandomMarketTrader>("to_remove", 10000000ULL);
    registry.remove_trader(id);
    EXPECT_FALSE(registry.trader_info(id).has_value());

    matcher.stop();
}
