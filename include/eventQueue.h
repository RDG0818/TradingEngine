// include/eventQueue.h

#ifndef TRADINGENGINE_INCLUDE_EVENTQUEUE_H_
#define TRADINGENGINE_INCLUDE_EVENTQUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include "order.h"
#include "utils.h"

// TODO: Consider using a lockless data structure

using EngineEvent = std::variant<std::unique_ptr<Order>, OrderID>; 

template<typename T>
class ThreadSafeQueue {
private:
  std::queue<T> queue_;
  mutable std::mutex mtx_;
  std::condition_variable cv_;

public:
  void push(T value) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(std::move(value));
    cv_.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]{ return !queue_.empty(); });
    T value = std::move(queue_.front());
    queue_.pop();
    return value;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.empty();
  }
};

#endif // TRADINGENGINE_INCLUDE_EVENTQUEUE_H_
