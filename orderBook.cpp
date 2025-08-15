#include "orderBook.h"


void OrderBook::addOrder(Order order) {
    if (allOrders.count(order.orderID) == 0) allOrders.insert({order.orderID, order});
    else throw std::invalid_argument("Order ID already exists. ID must be unique.");
    
    switch (order.side) {
        case Side::BUY:
            bids[order.intPrice].push_back(order.orderID);
            bid_quantities[order.intPrice] += order.quantity;
            break;
        case Side::SELL:
            asks[order.intPrice].push_back(order.orderID);
            ask_quantities[order.intPrice] += order.quantity;
            break;
    }
};


void OrderBook::cancelOrder(Order order) {
    if (allOrders.count(order.orderID) == 0) throw std::invalid_argument("Order does not exist.");
    else allOrders.erase(order.orderID);

    switch (order.side) {
        case Side::BUY: {
            std::deque<int>& bidDeque = bids[order.intPrice];
            bidDeque.erase(std::remove(bidDeque.begin(), bidDeque.end(), order.orderID), bidDeque.end());
            if (bidDeque.empty()) bids.erase(order.intPrice);
            bid_quantities[order.intPrice] -= order.quantity;
            if (bid_quantities[order.intPrice] == 0) bid_quantities.erase(order.intPrice);
            break;
        }
        case Side::SELL: {
            std::deque<int>& askDeque = asks[order.intPrice];
            askDeque.erase(std::remove(askDeque.begin(), askDeque.end(), order.orderID), askDeque.end());
            if (askDeque.empty()) asks.erase(order.intPrice);
            ask_quantities[order.intPrice] -= order.quantity;
            if (ask_quantities[order.intPrice] == 0) ask_quantities.erase(order.intPrice);
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
        auto bestBidQuantity = bid_quantities.rbegin();
        return MarketData(bestBid->first, bestBidQuantity->second);
    }
    throw std::invalid_argument("There are no bid orders.");
}


MarketData OrderBook::getAsk() {
    if (!asks.empty()) {
        auto bestAsk = asks.begin(); // the best ask is the lowest price
        auto bestAskQuantity = ask_quantities.begin();
        return MarketData(bestAsk->first, bestAskQuantity->second);
    }
        throw std::invalid_argument("There are no ask orders.");
}

