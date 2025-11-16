#include "trading_engine/orderBook.h"

void OrderBook::addOrder(std::unique_ptr<LimitOrder> order) {
    std::lock_guard<std::mutex> lock(mtx);

    const OrderID orderID = order->getOrderID();
    if (allOrders.count(orderID) > 0) {
        throw std::invalid_argument("Order ID already exists. ID must be unique.");
    }

    const Price price = order->getPrice();
    const Quantity quantity = order->getQuantity();
    const Side side = order->getSide();

    // Get or create the price level
    PriceLevel* priceLevel;
    if (side == Side::BUY) {
        priceLevel = &bids[price];
    } else {
        priceLevel = &asks[price];
    }

    // Add the order
    priceLevel->totalQuantity += quantity;
    priceLevel->orders.push_back(orderID);
    orderIterators[orderID] = std::prev(priceLevel->orders.end());

    allOrders[orderID] = std::move(order);
}

void OrderBook::removeOrder(OrderID orderID) {
    // This is a private method and must be called from within a lock.
    auto it = allOrders.find(orderID);
    if (it == allOrders.end()) {
        // Depending on the use case, we might want to log this instead of throwing.
        // For now, we assume internal calls are always valid.
        return;
    }

    Order* order = it->second.get();
    const Price price = order->getPrice();
    const Side side = order->getSide();

    auto& book = (side == Side::BUY) ? bids : asks;
    auto priceLevelIt = book.find(price);
    if (priceLevelIt == book.end()) {
        throw std::logic_error("Price level not found for an existing order.");
    }

    PriceLevel& priceLevel = priceLevelIt->second;
    priceLevel.totalQuantity -= order->getQuantity();
    
    auto orderIt = orderIterators.find(orderID);
    if (orderIt != orderIterators.end()) {
        priceLevel.orders.erase(orderIt->second);
        orderIterators.erase(orderIt);
    }

    if (priceLevel.orders.empty()) {
        book.erase(priceLevelIt);
    }

    allOrders.erase(it);
}

void OrderBook::cancelOrder(OrderID orderID) {
    std::lock_guard<std::mutex> lock(mtx);
    if (allOrders.count(orderID) == 0) {
        // Don't throw if the order doesn't exist, just return.
        return;
    }
    removeOrder(orderID);
}

Order* OrderBook::getOrder(OrderID id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = allOrders.find(id);
    if (it != allOrders.end()) {
        return it->second.get();
    }
    return nullptr;
}

void OrderBook::reduceOrderQuantity(OrderID orderID, Quantity quantityToReduce) {
    std::lock_guard<std::mutex> lock(mtx);

    auto allOrdersIt = allOrders.find(orderID);
    if (allOrdersIt == allOrders.end()) {
        return;
    }

    Order* order = allOrdersIt->second.get();
    if (quantityToReduce > order->getQuantity()) {
        // Or throw an exception, depending on desired behavior
        quantityToReduce = order->getQuantity();
    }

    order->setQuantity(order->getQuantity() - quantityToReduce);

    const Price price = order->getPrice();
    if (order->getSide() == Side::BUY) {
        bids.at(price).totalQuantity -= quantityToReduce;
    } else {
        asks.at(price).totalQuantity -= quantityToReduce;
    }

    if (order->getQuantity() == 0) {
        removeOrder(orderID);
    }
}

std::optional<MarketData> OrderBook::getBestBid() {
    std::lock_guard<std::mutex> lock(mtx);
    if (bids.empty()) {
        return std::nullopt;
    }
    const auto& [price, priceLevel] = *bids.begin();
    return MarketData{price, priceLevel.totalQuantity};
}

std::optional<MarketData> OrderBook::getBestAsk() {
    std::lock_guard<std::mutex> lock(mtx);
    if (asks.empty()) {
        return std::nullopt;
    }
    const auto& [price, priceLevel] = *asks.begin();
    return MarketData{price, priceLevel.totalQuantity};
}

bool OrderBook::isEmpty() {
    std::lock_guard<std::mutex> lock(mtx);
    return allOrders.empty();
}

bool OrderBook::isSideEmpty(Side side) {

    std::lock_guard<std::mutex> lock(mtx);

    return side == Side::BUY ? bids.empty() : asks.empty();

}



std::list<OrderID> OrderBook::getOrdersAtPrice(Price price) {

    std::lock_guard<std::mutex> lock(mtx);

    

    // Try to find the price level in the bids

    auto bidIt = bids.find(price);

    if (bidIt != bids.end()) {

        return bidIt->second.orders; // Return a copy

    }



    // If not in bids, try to find it in the asks

    auto askIt = asks.find(price);

    if (askIt != asks.end()) {

        return askIt->second.orders; // Return a copy

    }



    // If not found in either, return an empty list

    return {};

}
