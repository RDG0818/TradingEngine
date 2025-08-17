#pragma once

#include "orderBook.h"
#include "types.h"
#include "eventDispatcher.h"
#include "events.h"
#include <memory>

class MatchingEngine {
    private:
        OrderBook& book;
        EventDispatcher& dispatcher;
        OrderID nextOrderID;
        
        void matchOrder(Order* incomingOrder);
        void placeRestingLimitOrder(std::unique_ptr<LimitOrder> order);
        void createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity);

    public:
        MatchingEngine(OrderBook& orderBook, EventDispatcher& eventDispatcher);
        void processOrder(std::unique_ptr<Order> order);

};


