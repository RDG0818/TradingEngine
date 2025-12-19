#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <variant>
#include "order.h"
#include "utils.h"
#include <memory>

// TODO: Consider using a lockless data structure

using EngineEvent = std::variant<std::unique_ptr<Order>, OrderID>; // Is this necessary?

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(std::move(value));
        cv.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !queue.empty(); });
        T value = std::move(queue.front());
        queue.pop();
        return value;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};
