#pragma once
#include <map>
#include <list>
#include <algorithm>
#include <mutex>
#include <memory>
#include <optional>
#include <functional>
#include "order.h"
#include "limitOrder.h"
#include "types.h"

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

    std::mutex mtx;
    std::map<Price, PriceLevel, std::greater<Price>> bids; // Sorted high to low
    std::map<Price, PriceLevel> asks;                      // Sorted low to high
    std::unordered_map<OrderID, std::unique_ptr<Order>> allOrders;
    std::unordered_map<OrderID, std::list<OrderID>::iterator> orderIterators;

    void removeOrder(OrderID orderID);

public:

    void addOrder(std::unique_ptr<LimitOrder> order);

    void cancelOrder(OrderID orderID);

    Order* getOrder(OrderID orderID);

    void reduceOrderQuantity(OrderID orderID, Quantity quantityToReduce);

    std::optional<MarketData> getBestBid();

    std::optional<MarketData> getBestAsk();

    std::list<OrderID> getOrdersAtPrice(Price price);


    bool isEmpty();

    bool isSideEmpty(Side side);
};