#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "include/engine/event_bus.h"
#include "include/engine/order_matcher.h"
#include "include/trader_registry.h"
#include "include/engine/exchange_events.h"

TEST(TraderRegistry, StartsAndStopsCleanly) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher, bus, 640000000);
    registry.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    registry.stop();

    matcher.stop();
}

TEST(TraderRegistry, AddTraderAndReceivesFills) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher, bus, 640000000);
    registry.add_market_maker("mm1", 1000000000);
    registry.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    registry.stop();
    matcher.stop();
}

TEST(TraderRegistry, PauseStopsTicking) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher, bus, 640000000);
    registry.add_noise_trader("noise1", 1000000000, 5.0);
    registry.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    registry.pause_all();

    int updates_after_pause = 0;
    auto token = bus.subscribe<BookUpdateEvent>([&](const BookUpdateEvent&) {
        updates_after_pause++;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    bus.unsubscribe(token);

    EXPECT_EQ(updates_after_pause, 0);

    registry.stop();
    matcher.stop();
}

TEST(TraderRegistry, SetMarketMakerSpreadUpdatesSpread) {
    EventBus bus;
    OrderMatcher matcher(bus);
    matcher.start();

    TraderRegistry registry(matcher, bus, 640000000);
    registry.add_market_maker("mm1", 1000000000);
    registry.start();

    registry.set_market_maker_spread(50000);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    registry.stop();
    matcher.stop();
}
