// tests/cpp/eventDispatcherTest.cpp

#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "eventDispatcher.h"
#include "events.h" 
#include "gtest/gtest.h"

struct TestEventA : public BaseEvent {
  int value;
  TestEventA(int value) : value(value) {};
};

struct TestEventB : public BaseEvent {
  std::string message;
  TestEventB(std::string message) : message(message) {};
};

class EventDispatcherTest : public ::testing::Test {
protected:
  EventDispatcher dispatcher;
};


// Test Cases 

TEST_F(EventDispatcherTest, SubscribeAndPublishSingleEvent) {
  int receivedValue = 0;
  dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
    receivedValue = event.value;
  });

  dispatcher.publish(TestEventA(42));

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
  dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
    counter++;
  });

  dispatcher.publish(TestEventA(100));

  EXPECT_EQ(counter, 3);
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

  eventAReceived = false;

  dispatcher.publish(TestEventB{"test"});

  EXPECT_FALSE(eventAReceived);
  EXPECT_TRUE(eventBReceived);
}

TEST_F(EventDispatcherTest, PublishWithNoSubscribers) {
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

  // The dispatcher should catch the exception from the first subscriber and continue to call the next one.
  EXPECT_NO_THROW(dispatcher.publish(TestEventA{1}));

  EXPECT_TRUE(secondSubscriberWasCalled);
}

TEST_F(EventDispatcherTest, MultithreadedPublishStressTest) {
  std::atomic<int> event_count = 0;
  const int num_subscribers = 5;
  const int num_threads = 4;
  const int events_per_thread = 1000;

  for (int i = 0; i < num_subscribers; ++i) {
    dispatcher.subscribe<TestEventA>([&](const TestEventA& event) {
      event_count++;
    });
  }

  std::vector<std::thread> publisher_threads;
  for (int i = 0; i < num_threads; ++i) {
    publisher_threads.emplace_back([&]() {
      for (int j = 0; j < events_per_thread; ++j) {
        dispatcher.publish(TestEventA{j});
      }
    });
  }

  for (auto& t : publisher_threads) {
    t.join();
  }

  const int expected_count = num_subscribers * num_threads * events_per_thread;
  EXPECT_EQ(event_count, expected_count);
  std::cout << "Multithreaded test completed with " << event_count << " events handled." << std::endl;
}
