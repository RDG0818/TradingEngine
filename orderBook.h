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
    const std::list<OrderID>& orders;

    MarketData(Price p, Quantity q, const std::list<OrderID>& o) :
    price(p), quantity(q), orders(o) {};
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
        void cancelOrder(OrderID orderID);
        Order* getOrder(OrderID orderID);
        void reduceOrderQuantity(OrderID orderID, Quantity quantityToReduce);
        std::optional<MarketData> getBestBid();
        std::optional<MarketData> getBestAsk();
        bool isEmpty();
        bool isSideEmpty(Side side);
};