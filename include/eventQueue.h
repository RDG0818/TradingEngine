// include/eventQueue.h

#ifndef TRADINGENGINE_INCLUDE_EVENTQUEUE_H_
#define TRADINGENGINE_INCLUDE_EVENTQUEUE_H_

#include <memory>
#include <variant>

#include "order.h"
#include "utils.h"
#include "blockingconcurrentqueue.h"

// The queue now uses moodycamel's high-performance blocking concurrent queue
// instead of a standard queue with a mutex and condition variable.

using EngineEvent = std::variant<std::shared_ptr<Order>, OrderID>; 

template<typename T>
class ThreadSafeQueue {
private:
  moodycamel::BlockingConcurrentQueue<T> queue_;

public:
  void push(T value) {
    queue_.enqueue(std::move(value));
  }

  T pop() {
    T value;
    queue_.wait_dequeue(value);
    return value;
  }

  bool empty() const {
    // Note: size_approx() is not perfectly accurate in a concurrent environment,
    // but it is sufficient for a general "empty" check.
    return queue_.size_approx() == 0;
  }

  size_t size() const {
    return queue_.size_approx();
  }
};

#endif // TRADINGENGINE_INCLUDE_EVENTQUEUE_H_