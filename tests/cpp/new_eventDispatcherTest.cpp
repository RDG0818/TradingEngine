#include "gtest/gtest.h"
#include "trading_engine/eventDispatcher.h"
#include "trading_engine/events.h"
#include <iostream>
#include <atomic>
#include <thread>

// Concrete event classes for testing
struct TestEventA : public Event {
    int value;
    TestEventA(int v) : value(v) { type = EventType::ORDER_ACCEPTED; } // Using an existing type for simplicity
    std::string toString() const override { return "TestEventA"; }
};

struct TestEventB : public Event {
    std::string message;
    TestEventB(std::string m) : message(m) { type = EventType::ORDER_REJECTED; } // Using an existing type for simplicity
    std::string toString() const override { return "TestEventB"; }
};

// Mock listener for testing
class MockListener : public EventListener {
public:
    std::atomic<int> eventACount{0};
    std::atomic<int> eventBCount{0};
    std::atomic<bool> throw_on_event = false;

    void onEvent(const Event& event) override {
        if (throw_on_event) {
            throw std::runtime_error("Test exception");
        }
        if (event.type == EventType::ORDER_ACCEPTED) {
            eventACount++;
        } else if (event.type == EventType::ORDER_REJECTED) {
            eventBCount++;
        }
    }
};

class EventDispatcherTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    MockListener listener;
};

TEST_F(EventDispatcherTest, BasicSubscribeAndPublish) {
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);
    dispatcher.publish(TestEventA{42});
    EXPECT_EQ(listener.eventACount, 1);
}

TEST_F(EventDispatcherTest, MultipleSubscribersForSameEvent) {
    MockListener listener2;
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener2);
    dispatcher.publish(TestEventA{100});
    EXPECT_EQ(listener.eventACount, 1);
    EXPECT_EQ(listener2.eventACount, 1);
}

TEST_F(EventDispatcherTest, CorrectSubscriberForCorrectEventType) {
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);
    dispatcher.subscribe(EventType::ORDER_REJECTED, &listener);
    dispatcher.publish(TestEventA{1});
    EXPECT_EQ(listener.eventACount, 1);
    EXPECT_EQ(listener.eventBCount, 0);
    dispatcher.publish(TestEventB{"test"});
    EXPECT_EQ(listener.eventACount, 1);
    EXPECT_EQ(listener.eventBCount, 1);
}

TEST_F(EventDispatcherTest, PublishWithNoSubscribers) {
    EXPECT_NO_THROW(dispatcher.publish(TestEventA{99}));
}

TEST_F(EventDispatcherTest, SubscriberThrowsException) {
    listener.throw_on_event = true;
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);

    MockListener listener2;
    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener2);
    
    EXPECT_NO_THROW(dispatcher.publish(TestEventA{1}));
    EXPECT_EQ(listener2.eventACount, 1);
}

TEST_F(EventDispatcherTest, MultithreadedStressTest) {
    const int num_events = 1000;

    dispatcher.subscribe(EventType::ORDER_ACCEPTED, &listener);

    std::thread publisher([&]() {
        for (int i = 0; i < num_events; ++i) {
            dispatcher.publish(TestEventA{i});
        }
    });

    std::thread subscriber_thread([&]() {
        for (int i = 0; i < num_events; ++i) {
            MockListener* new_listener = new MockListener();
            dispatcher.subscribe(EventType::ORDER_ACCEPTED, new_listener);
            // Note: In a real app, you'd need to manage the lifecycle of these listeners
        }
    });

    publisher.join();
    subscriber_thread.join();

    EXPECT_GT(listener.eventACount, 0);
    std::cout << "Multithreaded test completed with " << listener.eventACount << " events handled by the initial listener." << std::endl;
}
