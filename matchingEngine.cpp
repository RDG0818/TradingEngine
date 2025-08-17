#include <matchingEngine.h>

MatchingEngine::MatchingEngine(OrderBook& orderBook, EventDispatcher& eventDispatcher)
    : book(orderBook), dispatcher(eventDispatcher), nextOrderID(1) {}

void MatchingEngine::processOrder(std::unique_ptr<Order> order) {
    order->setOrderID(nextOrderID++);
    matchOrder(order.get());    
    if (order->getQuantity() > 0) {
        if (order->getOrderType() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release()))); 
        }
        else {
            order->setOrderStatus(OrderStatus::CANCELLED);
            // TODO: Add OrderCancelled Event and logic for marketOrder
        }
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder) {
    // TODO: Finish this logic
}

void MatchingEngine::placeRestingLimitOrder(std::unique_ptr<LimitOrder> order) {
    order->setOrderStatus(OrderStatus::ACCEPTED);
    book.addOrder(std::move(order));
}

void MatchingEngine::createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {
    aggressor->setOrderStatus(aggressor->getQuantity() > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    resting->setOrderStatus(resting->getQuantity() > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    // TODO: Finish logic for TradeExecutedEvent
   // dispatcher.publish(TradeExecutedEvent{aggressor->getOrderID(), resting->getOrderID(), tradePrice, tradeQuantity});
}




