#include "matchingEngine.h"

MatchingEngine::MatchingEngine(OrderBook& orderBook, EventDispatcher& eventDispatcher)
    : book(orderBook), dispatcher(eventDispatcher), nextOrderID(1) {}

MatchingEngine::~MatchingEngine() {
    stop();
}

OrderID MatchingEngine::submitOrder(std::unique_ptr<Order> order) {
    OrderID id = nextOrderID.fetch_add(1);
    order->setOrderID(id);
    incoming_orders.push(std::move(order));
    return id;
}

void MatchingEngine::cancelOrder(OrderID orderID) {
    incoming_cancellations.push(orderID);
}

void MatchingEngine::start() {
    running = true;
    worker_thread = std::thread(&MatchingEngine::run_loop, this);
}

void MatchingEngine::stop() {
    running = false;
    incoming_orders.push(nullptr);
    incoming_cancellations.push(0);
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

void MatchingEngine::run_loop() {
    while (running) {
        std::unique_ptr<Order> order = incoming_orders.pop();
        
        if (order) processOrderSubmission(std::move(order));
        
        OrderID id_to_cancel = incoming_cancellations.pop();
        if (id_to_cancel != 0) {
            processOrderCancellation(id_to_cancel);
        }
    }
}

void MatchingEngine::processOrderSubmission(std::unique_ptr<Order> order) {
    if (order->getQuantity() == 0) {
        return;
    }

    matchOrder(order.get());    

    if (order->getQuantity() > 0) {
        if (order->getOrderType() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release()))); 
        }
        else {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getOrderID(), order->getQuantity()});
        }
    }
}

void MatchingEngine::processOrderCancellation(OrderID orderID) {
    try {
        book.cancelOrder(orderID);
    }
    catch (const std::invalid_argument& e) {
        // TODO: add some logic here
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder) {
    while (incomingOrder->getQuantity() > 0) {
        std::optional<MarketData> bestOpposingLevel = (incomingOrder->getSide() == Side::BUY) ? book.getBestAsk() : book.getBestBid();
        
        if (!bestOpposingLevel) break;

        if (incomingOrder->getOrderType() == OrderType::LIMIT) {
            auto limitOrder = static_cast<LimitOrder*>(incomingOrder);
            Price limitPrice = limitOrder->getPrice();
            Price bestOpposingPrice = bestOpposingLevel->price;

            if (incomingOrder->getSide() == Side::BUY && limitPrice < bestOpposingPrice) break;
            if (incomingOrder->getSide() == Side::SELL && limitPrice > bestOpposingPrice) break;
        }
        
        std::list<OrderID> restingOrderIDs = bestOpposingLevel->orders;

        for (OrderID restingOrderID : restingOrderIDs) {
           Order* genericRestingOrder = book.getOrder(restingOrderID); 
           if (!genericRestingOrder) continue;
           
           LimitOrder* restingOrder = static_cast<LimitOrder*>(genericRestingOrder);

           Quantity tradeQuantity = std::min(incomingOrder->getQuantity(), restingOrder->getQuantity());
           Price tradePrice = restingOrder->getPrice();

           createTrade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

           incomingOrder->setQuantity(incomingOrder->getQuantity() - tradeQuantity);
           book.reduceOrderQuantity(restingOrderID, tradeQuantity); 

           if (incomingOrder->getQuantity() == 0) return;
           
        }
    }
}

void MatchingEngine::placeRestingLimitOrder(std::unique_ptr<LimitOrder> order) {
    order->setOrderStatus(OrderStatus::ACCEPTED);
    book.addOrder(std::move(order));
}

void MatchingEngine::createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {
    Quantity aggressorRemaining = aggressor->getQuantity() - tradeQuantity;
    Quantity restingRemaining = resting->getQuantity() - tradeQuantity;
    aggressor->setOrderStatus(aggressorRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    resting->setOrderStatus(restingRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    dispatcher.publish(TradeExecutedEvent{aggressor->getSymbol(), tradePrice, tradeQuantity, 
        aggressor->getOrderID(), aggressor->getTraderID(), aggressor->getSide(), aggressor->getQuantity(), 
        resting->getOrderID(), resting->getTraderID(), resting->getQuantity()});
}




