#pragma once
#include <thread>
#include <atomic>
#include "orderBook.h"
#include "types.h"
#include "eventDispatcher.h"
#include "events.h"
#include <memory>
#include "threadSafeQueue.h"

class MatchingEngine {
    private:
        OrderBook& book;
        EventDispatcher& dispatcher;
        OrderID nextOrderID;

        std::thread worker_thread;
        std::atomic<bool> running{false};
        ThreadSafeQueue<std::unique_ptr<Order>> incoming_orders;

        void processOrderImpl(std::unique_ptr<Order> order);
        void matchOrder(Order* incomingOrder);
        void placeRestingLimitOrder(std::unique_ptr<LimitOrder> order);
        void createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity);

        void run_loop();

    public:
        MatchingEngine(OrderBook& orderBook, EventDispatcher& eventDispatcher);
        ~MatchingEngine();
        
        void submitOrder(std::unique_ptr<Order> order);
        void start();
        void stop();

};


