#pragma once
#include <map>
#include <list>
#include <algorithm>
#include <mutex>
#include <memory>
#include <optional>
#include "order.h"
#include "limitOrder.h"
#include "marketOrder.h"
#include "types.h"


struct MarketData {
    Price price;
    Quantity quantity;

    MarketData(Price p, Quantity q) :
    price(p), quantity(q) {};
};


class OrderBook {
    private:
        std::mutex mtx;
        std::map<Price, std::list<OrderID>> bids;
        std::map<Price, Quantity> bid_quantities;
        std::map<Price, std::list<OrderID>> asks;
        std::map<Price, Quantity> ask_quantities;
        std::unordered_map<OrderID, std::unique_ptr<Order>> allOrders;
        std::unordered_map<OrderID, std::list<OrderID>::iterator> orderIterators;
        
    public:
        void addOrder(std::unique_ptr<LimitOrder> order);
        void removeOrder(OrderID orderID);
        std::optional<MarketData> getBestBid();
        std::optional<MarketData> getBestAsk();
        bool isEmpty();
};