#include "trading_engine/matchingEngine.h"
#include "trading_engine/orderFactory.h"
#include <variant>

MatchingEngine::MatchingEngine(EventDispatcher& eventDispatcher)
    : dispatcher(eventDispatcher), nextOrderID(1) {}

MatchingEngine::~MatchingEngine() {
    stop();
}

OrderID MatchingEngine::submitOrder(const RawOrderParams& params) {
    OrderID id = nextOrderID.fetch_add(1);
    
    SymbolID symbolID = SymbolRegistry::getInstance().getID(params.symbol);
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_[id] = symbolID;
    }

    auto order = OrderFactory::createOrder(params, id);
    event_queue.push(std::move(order));
    return id;
}

void MatchingEngine::cancelOrder(OrderID orderID) {
    event_queue.push(orderID);
}

void MatchingEngine::start() {
    running = true;
    worker_thread = std::thread(&MatchingEngine::run_loop, this);
}

void MatchingEngine::stop() {
    running = false;
    event_queue.push(OrderID{0}); // Use a dummy event to wake up the thread
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
        EngineEvent event = event_queue.pop();
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<Order>>) {
                if (arg) processOrderSubmission(std::move(arg));
            } else if constexpr (std::is_same_v<T, OrderID>) {
                if (arg != 0) processOrderCancellation(arg);
            }
        }, std::move(event));
    }
}

void MatchingEngine::processOrderSubmission(std::unique_ptr<Order> order) {
    if (order->getQuantity() == 0) {
        return;
    }

    if (order->getOrderType() == OrderType::STOP_MARKET) {
        processStopMarketOrder(std::unique_ptr<StopMarketOrder>(static_cast<StopMarketOrder*>(order.release())));
        return;
    } else if (order->getOrderType() == OrderType::STOP_LIMIT) {
        processStopLimitOrder(std::unique_ptr<StopLimitOrder>(static_cast<StopLimitOrder*>(order.release())));
        return;
    }

    OrderBook* book = getOrCreateOrderBook(order->getSymbolID());
    matchOrder(order.get(), *book);

    if (order->getQuantity() > 0) {
        if (order->getTimeInForce() == TimeInForce::IOC || order->getTimeInForce() == TimeInForce::FOK) {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getOrderID(), order->getQuantity()});
        } else if (order->getOrderType() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release())), *book); 
        } else {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getOrderID(), order->getQuantity()});
        }
    }
}

void MatchingEngine::processStopMarketOrder(std::unique_ptr<StopMarketOrder> order) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->getSide() == Side::BUY) {
        stop_buy_orders_[order->getStopPrice()].push_back(order.release());
    } else {
        stop_sell_orders_[order->getStopPrice()].push_back(order.release());
    }
}

void MatchingEngine::processStopLimitOrder(std::unique_ptr<StopLimitOrder> order) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->getSide() == Side::BUY) {
        stop_buy_orders_[order->getStopPrice()].push_back(order.release());
    } else {
        stop_sell_orders_[order->getStopPrice()].push_back(order.release());
    }
}

void MatchingEngine::processOrderCancellation(OrderID orderID) {
    SymbolID symbolID;
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        auto it = orderID_to_symbol_.find(orderID);
        if (it == orderID_to_symbol_.end()) {
            return;
        }
        symbolID = it->second;
    }

    OrderBook* book = getBook(symbolID);
    if (book) {
        try {
            book->cancelOrder(orderID);
            std::lock_guard<std::mutex> lock(order_map_mutex_);
            orderID_to_symbol_.erase(orderID);
        } catch (const std::invalid_argument& e) {
            // TODO: add logging
        }
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder, OrderBook& book) {
    if (incomingOrder->getTimeInForce() == TimeInForce::FOK) {
        Quantity availableQuantity = 0;
        if (incomingOrder->getSide() == Side::BUY) {
            if (book.getBestAsk().has_value()) {
                auto bestAsk = book.getBestAsk().value();
                if (incomingOrder->getPrice() >= bestAsk.price) {
                    availableQuantity = book.getOrdersAtPrice(bestAsk.price).size() * book.getOrder(book.getOrdersAtPrice(bestAsk.price).front())->getQuantity();
                }
            }
        } else {
            if (book.getBestBid().has_value()) {
                auto bestBid = book.getBestBid().value();
                if (incomingOrder->getPrice() <= bestBid.price) {
                    availableQuantity = book.getOrdersAtPrice(bestBid.price).size() * book.getOrder(book.getOrdersAtPrice(bestBid.price).front())->getQuantity();
                }
            }
        }
        if (incomingOrder->getQuantity() > availableQuantity) {
            return;
        }
    }

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

    if (aggressorRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(aggressor->getOrderID());
    }
    if (restingRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(resting->getOrderID());
    }

    triggerStopOrders(tradePrice);
}

void MatchingEngine::triggerStopOrders(Price lastTradePrice) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);

    // Trigger buy stop orders
    for (auto it = stop_buy_orders_.begin(); it != stop_buy_orders_.end() && it->first <= lastTradePrice; ) {
        for (auto& order : it->second) {
            if (order->getOrderType() == OrderType::STOP_MARKET) {
                event_queue.push(std::make_unique<MarketOrder>(order->getSymbolID(), order->getOrderID(), order->getSide(), order->getQuantity(), order->getTraderID()));
            } else if (order->getOrderType() == OrderType::STOP_LIMIT) {
                event_queue.push(std::make_unique<LimitOrder>(order->getSymbolID(), order->getOrderID(), order->getSide(), order->getPrice(), order->getQuantity(), order->getTraderID()));
            }
        }
        it = stop_buy_orders_.erase(it);
    }

    // Trigger sell stop orders
    for (auto it = stop_sell_orders_.begin(); it != stop_sell_orders_.end() && it->first >= lastTradePrice; ) {
        for (auto& order : it->second) {
            if (order->getOrderType() == OrderType::STOP_MARKET) {
                event_queue.push(std::make_unique<MarketOrder>(order->getSymbolID(), order->getOrderID(), order->getSide(), order->getQuantity(), order->getTraderID()));
            } else if (order->getOrderType() == OrderType::STOP_LIMIT) {
                event_queue.push(std::make_unique<LimitOrder>(order->getSymbolID(), order->getOrderID(), order->getSide(), order->getPrice(), order->getQuantity(), order->getTraderID()));
            }
        }
        it = stop_sell_orders_.erase(it);
    }
}
