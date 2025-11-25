#pragma once
#include <thread>
#include <atomic>
#include <unordered_map>
#include "orderBook.h"
#include "utils.h"
#include "eventDispatcher.h"
#include "events.h"
#include <memory>
#include "eventQueue.h"
#include "utils.h"
#include "orderFactory.h"
#include <list>

class MatchingEngine {
private:
    EventDispatcher& dispatcher;
    std::atomic<OrderID> nextOrderID;

    std::unordered_map<SymbolID, std::unique_ptr<OrderBook>> books_;
    mutable std::mutex books_mutex_; 

    std::unordered_map<OrderID, SymbolID> orderID_to_symbol_;
    mutable std::mutex order_map_mutex_; 

    std::thread worker_thread;
    std::atomic<bool> running{false};
    ThreadSafeQueue<EngineEvent> event_queue;

    std::map<Price, std::list<Order*>> stop_buy_orders_;
    std::map<Price, std::list<Order*>> stop_sell_orders_;
    mutable std::mutex stop_orders_mutex_;

    void run_loop();
    void processOrderSubmission(std::unique_ptr<Order> order);
    void processOrderCancellation(OrderID orderID);
    void processStopMarketOrder(std::unique_ptr<StopMarketOrder> order);
    void processStopLimitOrder(std::unique_ptr<StopLimitOrder> order);
    void matchOrder(Order* incomingOrder, OrderBook& book);
    void placeRestingLimitOrder(std::unique_ptr<LimitOrder> order, OrderBook& book);
    void createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity);
    void triggerStopOrders(Price lastTradePrice);
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


