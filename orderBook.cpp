#include "orderBook.h"


void OrderBook::addOrder(Order order) {
    switch (order.side) {
        case Side::BUY:
            bids[order.price].push_back(order.orderID);
            break;
        case Side::SELL:
            asks[order.price].push_back(order.orderID);
            break;
    }
};


void OrderBook::cancelOrder(Order order) {
    switch (order.side) {
        case Side::BUY: {
            std::deque<int>& bidDeque = bids[order.price];
            bidDeque.erase(std::remove(bidDeque.begin(), bidDeque.end(), order.orderID), bidDeque.end());
            if (bidDeque.empty()) bids.erase(order.price);
            break;
        }
        case Side::SELL: {
            std::deque<int>& askDeque = asks[order.price];
            askDeque.erase(std::remove(askDeque.begin(), askDeque.end(), order.orderID), askDeque.end());
            if (askDeque.empty()) asks.erase(order.price);
            break;
        }
    }
};


void OrderBook::modifyOrder(Order old_order, Order new_order) {
    cancelOrder(old_order);
    addOrder(new_order);
}


MarketData OrderBook::getBid() {
    if (!bids.empty()) {
        auto bestBid = bids.rbegin(); // the best bid is the highest price
        return MarketData(bestBid->first, bestBid->second.size());
    }
    return MarketData(0, 0); 
}


MarketData OrderBook::getAsk() {
    if (!asks.empty()) {
        auto bestAsk = asks.begin(); // the best ask is the lowest price
        return MarketData(bestAsk->first, bestAsk->second.size());
    }
    return MarketData(0, 0); 
}

