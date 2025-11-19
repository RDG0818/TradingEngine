#pragma once
#include <map>
#include <list>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <optional>
#include <functional>
#include "order.h"
#include "limitOrder.h"
#include "types.h"

// TODO: Utilize lockless data structures here

struct MarketData {
    Price price;
    Quantity quantity;
};

class OrderBook {
private:
    struct PriceLevel {
        Quantity totalQuantity;
        std::list<OrderID> orders;
    };

    mutable std::shared_mutex bids_mtx;
    mutable std::shared_mutex asks_mtx;
    mutable std::mutex orders_mtx;

    std::map<Price, PriceLevel, std::greater<Price>> bids; // Sorted high to low
    std::map<Price, PriceLevel> asks;                      // Sorted low to high
    std::unordered_map<OrderID, std::unique_ptr<Order>> allOrders;
    std::unordered_map<OrderID, std::list<OrderID>::iterator> orderIterators;


public:

    void addOrder(std::unique_ptr<LimitOrder> order);

    void cancelOrder(OrderID orderID);
    
    void removeOrder(OrderID orderID);

    Order* getOrder(OrderID orderID);

    void reduceOrderQuantity(OrderID orderID, Quantity quantityToReduce);

    std::optional<MarketData> getBestBid();

    std::optional<MarketData> getBestAsk();

    std::list<OrderID> getOrdersAtPrice(Price price);


    bool isEmpty();

    bool isSideEmpty(Side side);
};
