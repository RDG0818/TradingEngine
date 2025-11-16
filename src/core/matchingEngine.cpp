#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderFactory.h"

MatchingEngine::MatchingEngine(EventDispatcher& eventDispatcher)
    : dispatcher(eventDispatcher), nextOrderID(1) {}

MatchingEngine::~MatchingEngine() {
    stop();
}

OrderID MatchingEngine::submitOrder(const RawOrderParams& params) {
    OrderID id = nextOrderID.fetch_add(1);
    
    // Store the mapping from OrderID to SymbolID
    SymbolID symbolID = SymbolRegistry::getInstance().getID(params.symbol);
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_[id] = symbolID;
    }

    auto order = OrderFactory::createOrder(params, id);
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

OrderBook* MatchingEngine::getOrCreateOrderBook(SymbolID symbolID) {
    std::lock_guard<std::mutex> lock(books_mutex_);
    auto it = books_.find(symbolID);
    if (it == books_.end()) {
        books_[symbolID] = std::make_unique<OrderBook>();
        return books_[symbolID].get();
    }
    return it->second.get();
}

OrderBook* MatchingEngine::getBook(SymbolID symbolID) {
    std::lock_guard<std::mutex> lock(books_mutex_);
    auto it = books_.find(symbolID);
    if (it == books_.end()) {
        return nullptr;
    }
    return it->second.get();
}

void MatchingEngine::run_loop() {
    while (running) {
        std::unique_ptr<Order> order = incoming_orders.pop();
        if (order) {
            processOrderSubmission(std::move(order));
        }
        
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

    OrderBook* book = getOrCreateOrderBook(order->getSymbolID());
    matchOrder(order.get(), *book);

    if (order->getQuantity() > 0) {
        if (order->getOrderType() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release())), *book); 
        } else {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getOrderID(), order->getQuantity()});
        }
    }
}

void MatchingEngine::processOrderCancellation(OrderID orderID) {
    SymbolID symbolID;
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        auto it = orderID_to_symbol_.find(orderID);
        if (it == orderID_to_symbol_.end()) {
            // Order not found, maybe already filled and removed
            return;
        }
        symbolID = it->second;
    }

    OrderBook* book = getBook(symbolID);
    if (book) {
        try {
            book->cancelOrder(orderID);
            // After a successful cancellation, we can remove the ID from our map
            std::lock_guard<std::mutex> lock(order_map_mutex_);
            orderID_to_symbol_.erase(orderID);
        } catch (const std::invalid_argument& e) {
            // TODO: add logging
        }
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder, OrderBook& book) {
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
        
        // This is a temporary copy. In a real high-performance scenario, you'd want to avoid this.
        std::list<OrderID> restingOrderIDs = book.getOrdersAtPrice(bestOpposingLevel->price);

        for (OrderID restingOrderID : restingOrderIDs) {
           Order* restingOrder = book.getOrder(restingOrderID); 
           if (!restingOrder) continue;
           
           Quantity tradeQuantity = std::min(incomingOrder->getQuantity(), restingOrder->getQuantity());
           Price tradePrice = restingOrder->getPrice();

           createTrade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

           incomingOrder->setQuantity(incomingOrder->getQuantity() - tradeQuantity);
           book.reduceOrderQuantity(restingOrderID, tradeQuantity); 

           if (incomingOrder->getQuantity() == 0) return;
        }
    }
}

void MatchingEngine::placeRestingLimitOrder(std::unique_ptr<LimitOrder> order, OrderBook& book) {
    order->setOrderStatus(OrderStatus::ACCEPTED);
    book.addOrder(std::move(order));
}

void MatchingEngine::createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {
    Quantity aggressorRemaining = aggressor->getQuantity() - tradeQuantity;
    Quantity restingRemaining = resting->getQuantity() - tradeQuantity;
    aggressor->setOrderStatus(aggressorRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    resting->setOrderStatus(restingRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    
    dispatcher.publish(TradeExecutedEvent{aggressor->getSymbolID(), tradePrice, tradeQuantity, 
        aggressor->getOrderID(), aggressor->getTraderID(), aggressor->getSide(), aggressor->getQuantity(), 
        resting->getOrderID(), resting->getTraderID(), resting->getQuantity()});

    // After a full fill, the order can be removed from the tracking map
    if (aggressorRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(aggressor->getOrderID());
    }
    if (restingRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(resting->getOrderID());
    }
}
