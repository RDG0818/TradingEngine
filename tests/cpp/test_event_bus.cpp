#include <gtest/gtest.h>
#include "include/event_bus.h"
#include "include/core/order.h"

struct PriceUpdate { Price price; };

TEST(EventBus, SubscribeAndPublish) {
    EventBus bus;
    int call_count = 0;
    Price received = 0;

    bus.subscribe<PriceUpdate>([&](const PriceUpdate& e) {
        ++call_count;
        received = e.price;
    });

    bus.publish(PriceUpdate{642000000ULL});
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(received, 642000000ULL);
}

TEST(EventBus, MultipleSubscribers) {
    EventBus bus;
    int a = 0, b = 0;
    bus.subscribe<PriceUpdate>([&](const PriceUpdate&) { ++a; });
    bus.subscribe<PriceUpdate>([&](const PriceUpdate&) { ++b; });
    bus.publish(PriceUpdate{1ULL});
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

TEST(EventBus, UnsubscribeStopsDelivery) {
    EventBus bus;
    int count = 0;
    auto token = bus.subscribe<PriceUpdate>([&](const PriceUpdate&) { ++count; });
    bus.publish(PriceUpdate{1ULL});
    bus.unsubscribe(token);
    bus.publish(PriceUpdate{2ULL});
    EXPECT_EQ(count, 1);
}

TEST(EventBus, DifferentEventTypesDontInterfere) {
    struct OtherEvent { int x; };
    EventBus bus;
    int price_count = 0, other_count = 0;
    bus.subscribe<PriceUpdate>([&](const PriceUpdate&) { ++price_count; });
    bus.subscribe<OtherEvent>([&](const OtherEvent&)   { ++other_count; });
    bus.publish(PriceUpdate{1ULL});
    EXPECT_EQ(price_count, 1);
    EXPECT_EQ(other_count, 0);
}
