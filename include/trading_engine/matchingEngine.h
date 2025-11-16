#pragma once
#include <thread>
#include <atomic>
#include <unordered_map>
#include "orderBook.h"
#include "types.h"
#include "eventDispatcher.h"
#include "events.h"
#include <memory>
#include "threadSafeQueue.h"
#include "orderFactory.h"

class MatchingEngine {
private:
    // --- Core Components ---
    EventDispatcher& dispatcher;
    std::atomic<OrderID> nextOrderID;

    // --- Data Structures for Multi-Instrument Support ---
    std::unordered_map<SymbolID, std::unique_ptr<OrderBook>> books_;
    mutable std::mutex books_mutex_; // Protects the books_ map

    std::unordered_map<OrderID, SymbolID> orderID_to_symbol_;
    mutable std::mutex order_map_mutex_; // Protects the orderID_to_symbol_ map

    // --- Worker Thread and Queues ---
    std::thread worker_thread;
    std::atomic<bool> running{false};
    ThreadSafeQueue<OrderID> incoming_cancellations;
    ThreadSafeQueue<std::unique_ptr<Order>> incoming_orders;

    // --- Private Methods ---
    void run_loop();
    void processOrderSubmission(std::unique_ptr<Order> order);
    void processOrderCancellation(OrderID orderID);
    void matchOrder(Order* incomingOrder, OrderBook& book);
    void placeRestingLimitOrder(std::unique_ptr<LimitOrder> order, OrderBook& book);
    void createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity);
    OrderBook* getOrCreateOrderBook(SymbolID symbolID);

public:
    MatchingEngine(EventDispatcher& eventDispatcher);
    ~MatchingEngine();
    
    OrderID submitOrder(const RawOrderParams& params);
    void cancelOrder(OrderID orderID);
    
    void start();
    void stop();

    // --- Test-only Methods ---
    OrderBook* getBook(SymbolID symbolID);
};


