#include "trading_engine/orderBook.h"

void OrderBook::addOrder(std::unique_ptr<LimitOrder> order) {
    const Side side = order->getSide();
    const OrderID orderID = order->getOrderID();
    const Price price = order->getPrice();
    const Quantity quantity = order->getQuantity();

    if (side == Side::BUY) {
        std::scoped_lock lock(orders_mtx, bids_mtx);
        if (allOrders.count(orderID) > 0) {
            throw std::invalid_argument("Order ID already exists. ID must be unique.");
        }

        PriceLevel& priceLevel = bids[price];
        priceLevel.totalQuantity += quantity;
        priceLevel.orders.push_back(orderID);
        orderIterators[orderID] = std::prev(priceLevel.orders.end());
        allOrders[orderID] = std::move(order);
    } else {
        std::scoped_lock lock(orders_mtx, asks_mtx);
        if (allOrders.count(orderID) > 0) {
            throw std::invalid_argument("Order ID already exists. ID must be unique.");
        }

        PriceLevel& priceLevel = asks[price];
        priceLevel.totalQuantity += quantity;
        priceLevel.orders.push_back(orderID);
        orderIterators[orderID] = std::prev(priceLevel.orders.end());
        allOrders[orderID] = std::move(order);
    }
}

void OrderBook::removeOrder(std::unordered_map<OrderID, std::unique_ptr<Order>>::iterator allOrdersIt) {
    Order* order = allOrdersIt->second.get(); // Error checking handled in cancelOrder
    const OrderID orderID = order->getOrderID();
    const Price price = order->getPrice();
    const Side side = order->getSide();
    const Quantity quantity = order->getQuantity();

    if (side == Side::BUY) {
        auto priceLevelIt = bids.find(price);
        if (priceLevelIt != bids.end()) {
            PriceLevel& priceLevel = priceLevelIt->second;
            priceLevel.totalQuantity -= quantity;
            
            auto orderIt = orderIterators.find(orderID);
            if (orderIt != orderIterators.end()) {
                priceLevel.orders.erase(orderIt->second);
                orderIterators.erase(orderIt);
            }

            if (priceLevel.orders.empty()) {
                bids.erase(priceLevelIt);
            }
        } 
    } else { // Side::SELL
        auto priceLevelIt = asks.find(price);
        if (priceLevelIt != asks.end()) {
            PriceLevel& priceLevel = priceLevelIt->second;
            priceLevel.totalQuantity -= quantity;
            
            auto orderIt = orderIterators.find(orderID);
            if (orderIt != orderIterators.end()) {
                priceLevel.orders.erase(orderIt->second);
                orderIterators.erase(orderIt);
            }

            if (priceLevel.orders.empty()) {
                asks.erase(priceLevelIt);
            }
        } 
    }

    allOrders.erase(allOrdersIt);
}

void OrderBook::cancelOrder(OrderID orderID) {
    std::unique_lock<std::mutex> orders_lock(orders_mtx);
    auto allOrdersIt = allOrders.find(orderID);
    if (allOrdersIt == allOrders.end()) {
        return;
    }

    if (allOrdersIt->second->getSide() == Side::BUY) {
        std::unique_lock<std::shared_mutex> bids_lock(bids_mtx);
        removeOrder(allOrdersIt);
    } else {
        std::unique_lock<std::shared_mutex> asks_lock(asks_mtx);
        removeOrder(allOrdersIt);
    }
}

Order* OrderBook::getOrder(OrderID id) {
    std::lock_guard<std::mutex> lock(orders_mtx);
    auto it = allOrders.find(id);
    if (it != allOrders.end()) {
        return it->second.get();
    }
    return nullptr;
}

void OrderBook::reduceOrderQuantity(OrderID orderID, Quantity quantityToReduce) {
    std::unique_lock<std::mutex> orders_lock(orders_mtx);
    auto allOrdersIt = allOrders.find(orderID);
    if (allOrdersIt == allOrders.end()) {
        return;
    }

    Order* order = allOrdersIt->second.get();
    const Price price = order->getPrice();
    const Side side = order->getSide();

    if (side == Side::BUY) {
        std::unique_lock<std::shared_mutex> bids_lock(bids_mtx);
        if (quantityToReduce > order->getQuantity()) {
            quantityToReduce = order->getQuantity();
        }
        order->setQuantity(order->getQuantity() - quantityToReduce);
        bids.at(price).totalQuantity -= quantityToReduce;
        if (order->getQuantity() == 0) {
            removeOrder(allOrdersIt);
        }
    } else {
        std::unique_lock<std::shared_mutex> asks_lock(asks_mtx);
        if (quantityToReduce > order->getQuantity()) {
            quantityToReduce = order->getQuantity();
        }
        order->setQuantity(order->getQuantity() - quantityToReduce);
        asks.at(price).totalQuantity -= quantityToReduce;
        if (order->getQuantity() == 0) {
            removeOrder(allOrdersIt);
        }
    }
}

std::optional<MarketData> OrderBook::getBestBid() {
    std::shared_lock<std::shared_mutex> lock(bids_mtx);
    if (bids.empty()) {
        return std::nullopt;
    }
    const auto& [price, priceLevel] = *bids.begin();
    return MarketData{price, priceLevel.totalQuantity};
}

std::optional<MarketData> OrderBook::getBestAsk() {
    std::shared_lock<std::shared_mutex> lock(asks_mtx);
    if (asks.empty()) {
        return std::nullopt;
    }
    const auto& [price, priceLevel] = *asks.begin();
    return MarketData{price, priceLevel.totalQuantity};
}

bool OrderBook::isEmpty() {
    std::lock_guard<std::mutex> lock(orders_mtx);
    return allOrders.empty();
}

bool OrderBook::isSideEmpty(Side side) {
    if (side == Side::BUY) {
        std::shared_lock<std::shared_mutex> lock(bids_mtx);
        return bids.empty();
    } else {
        std::shared_lock<std::shared_mutex> lock(asks_mtx);
        return asks.empty();
    }
}

bool OrderBook::forEachOrderAtPrice(Price price, Side side, const std::function<bool(OrderID)>& callback) {
    std::list<OrderID> orders_at_price;
    if (side == Side::BUY) { 
        std::shared_lock<std::shared_mutex> lock(bids_mtx);
        auto bidIt = bids.find(price);
        if (bidIt != bids.end()) {
            orders_at_price.assign(bidIt->second.orders.begin(), bidIt->second.orders.end()); // copied by value
        }
    } else { // Side::SELL 
        std::shared_lock<std::shared_mutex> lock(asks_mtx);
        auto askIt = asks.find(price);
        if (askIt != asks.end()) {
            orders_at_price.assign(askIt->second.orders.begin(), askIt->second.orders.end()); // copied by value
        }
    }

    // Now iterate over the copied IDs, safe from modifications to the original list
    for (const auto& orderID : orders_at_price) {
        if (!callback(orderID)) {
            return false; // Stop iterating and return false if callback says so
        }
    }
    return true; // All orders processed or no orders at price, continue
}
