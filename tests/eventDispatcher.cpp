#include "gtest/gtest.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <typeindex>
#include <iostream>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include "eventDispatcher.h"

struct TestEventA {
    int value;
};

struct TestEventB {
    std::string message;
};

// Test fixture for the EventDispatcher
class EventDispatcherTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
};


// --- Test Cases ---

TEST_F(EventDispatcherTest, BasicSubscribeAndPublish) {
    int receivedValue = 0;
    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
        receivedValue = event.value;
    });

    dispatcher.publish(TestEventA{42});

    EXPECT_EQ(receivedValue, 42);
}

TEST_F(EventDispatcherTest, MultipleSubscribersForSameEvent) {
    std::atomic<int> counter = 0;
    
    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
        counter++;
    });
    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
        counter++;
    });

    dispatcher.publish(TestEventA{100});

    EXPECT_EQ(counter, 2);
}

TEST_F(EventDispatcherTest, CorrectSubscriberForCorrectEventType) {
    bool eventAReceived = false;
    bool eventBReceived = false;

    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
        eventAReceived = true;
    });
    dispatcher.subscribe<TestEventB>([&](const TestEventB& event) {
        eventBReceived = true;
    });

    dispatcher.publish(TestEventA{1});

    EXPECT_TRUE(eventAReceived);
    EXPECT_FALSE(eventBReceived);
}

TEST_F(EventDispatcherTest, PublishWithNoSubscribers) {
    // This test simply ensures that publishing with no subscribers
    // does not cause any errors or crashes.
    EXPECT_NO_THROW(dispatcher.publish(TestEventA{99}));
}

TEST_F(EventDispatcherTest, SubscriberThrowsException) {
    std::atomic<bool> secondSubscriberWasCalled = false;

    dispatcher.subscribe<TestEventA>([](const TestEventA& event) {
        throw std::runtime_error("Test exception");
    });
    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
        secondSubscriberWasCalled = true;
    });

    // We expect no exceptions to escape the publish method.
    EXPECT_NO_THROW(dispatcher.publish(TestEventA{1}));

    // We expect the second subscriber to still be called even after the first one threw.
    EXPECT_TRUE(secondSubscriberWasCalled);
}

TEST_F(EventDispatcherTest, MultithreadedStressTest) {
    std::atomic<int> eventCount = 0;
    const int num_events = 1000;

    // Publisher thread: Rapidly publishes events.
    std::thread publisher([&]() {
        for (int i = 0; i < num_events; ++i) {
            dispatcher.publish(TestEventA{i});
        }
    });

    // Subscriber thread: Rapidly subscribes new listeners.
    std::thread subscriber([&]() {
        for (int i = 0; i < num_events; ++i) {
            dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
                eventCount++;
            });
        }
    });

    publisher.join();
    subscriber.join();

    // The final count is unpredictable due to race conditions of when
    // subscriptions are added vs. when events are published.
    // The key purpose of this test is to ensure that the dispatcher
    // can handle concurrent access without crashing or deadlocking.
    // A non-zero count proves the system is fundamentally working.
    EXPECT_GT(eventCount, 0);
    std::cout << "Multithreaded test completed with " << eventCount << " events handled." << std::endl;
}
