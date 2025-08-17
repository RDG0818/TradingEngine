#include "orderBook.h"


void OrderBook::addOrder(std::unique_ptr<LimitOrder> order) {
    std::lock_guard<std::mutex> lock(mtx);

    const OrderID orderID = order->getOrderID();
    const Price price = order->getPrice();
    const Quantity quantity = order->getQuantity();
    const Side side = order->getSide();

    if (allOrders.count(order->getOrderID()) > 0) 
        throw std::invalid_argument("Order ID already exists. ID must be unique.");
    
    switch (side) {
        case Side::BUY:
            bids[price].push_back(orderID);
            orderIterators[orderID] = std::prev(bids[price].end());
            bid_quantities[price] += quantity;
            break;
        case Side::SELL:
            asks[price] .push_back(orderID);
            orderIterators[orderID] = std::prev(asks[price].end());
            ask_quantities[price] += quantity;
            break;
    }

    allOrders[order->getOrderID()] = std::move(order);
};


void OrderBook::removeOrder(OrderID orderID) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = allOrders.find(orderID);
    if (it == allOrders.end()) {
        throw std::invalid_argument("Order to cancel does not exist.");
    };
    

    auto iter_it = orderIterators.find(orderID);
    if (iter_it == orderIterators.end()) {
        throw std::logic_error("Order exists but iterator does not.");
    }
    
    Order* order = it->second.get();
    auto limitOrder = static_cast<LimitOrder*>(order); 
    Price price = limitOrder->getPrice();
    Quantity quantity = limitOrder->getQuantity(); 
    
    switch (order->getSide()) {
        case Side::BUY: {
            std::list<OrderID>& bidList = bids.at(price);
            bidList.erase(iter_it->second);
            if (bidList.empty()) bids.erase(price);
            bid_quantities.at(price) -= quantity;
            if (bid_quantities.at(price) == 0) bid_quantities.erase(price);
            break;
        }
        case Side::SELL: {
            std::list<OrderID>& askList = asks.at(price);
            askList.erase(iter_it->second);
            if (askList.empty()) asks.erase(price);
            ask_quantities.at(price) -= quantity;
            if (ask_quantities.at(price) == 0) ask_quantities.erase(price);
            break;
        }
    }
        
    orderIterators.erase(iter_it);
    allOrders.erase(it);
};


std::optional<MarketData> OrderBook::getBestBid() {
    std::lock_guard<std::mutex> lock(mtx);
    if (bids.empty()) {
        return std::nullopt;
    }
    const auto& [price, orders] = *bids.rbegin();
    return MarketData(price, bid_quantities.at(price));
}


std::optional<MarketData> OrderBook::getBestAsk() {
    std::lock_guard<std::mutex> lock(mtx);
    if (asks.empty()) {
        return std::nullopt;
    }
    const auto& [price, orders] = *asks.begin();
    return MarketData(price, ask_quantities.at(price));
}

bool OrderBook::isEmpty() {
    return allOrders.empty();
}
