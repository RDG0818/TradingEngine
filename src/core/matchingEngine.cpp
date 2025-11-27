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
    
    try {
        SymbolID symbolID = SymbolRegistry::getInstance().getID(params.symbol);
        {
            std::lock_guard<std::mutex> lock(order_map_mutex_);
            orderID_to_symbol_[id] = symbolID;
        }

        auto order = OrderFactory::createOrder(params, id);
        event_queue.push(std::move(order));
    } catch (const InvalidPriceException& e) {
        dispatcher.publish(OrderRejectedEvent{id, params.traderID, RejectionReason::INVALID_PRICE, e.what()});
    } catch (const InvalidQuantityException& e) {
        dispatcher.publish(OrderRejectedEvent{id, params.traderID, RejectionReason::INVALID_QUANTITY, e.what()});
    } catch (const UnsupportedOrderTypeException& e) {
        dispatcher.publish(OrderRejectedEvent{id, params.traderID, RejectionReason::UNSUPPORTED_ORDER_TYPE, e.what()});
    } catch (const std::invalid_argument& e) {
        dispatcher.publish(OrderRejectedEvent{id, params.traderID, RejectionReason::OTHER, e.what()});
    }
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
        dispatcher.publish(OrderRejectedEvent{order->getOrderID(), order->getTraderID(), RejectionReason::INVALID_QUANTITY, "Quantity must be positive."});
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

    // Level 1/2 Feed Dissemination
    
    auto prevBestBid = book->getBestBid();
    auto prevBestAsk = book->getBestAsk();

    matchOrder(order.get(), *book);

    auto currentBestBid = book->getBestBid();
    auto currentBestAsk = book->getBestAsk();

    bool bidChanged = (prevBestBid.has_value() != currentBestBid.has_value()) || 
                  (currentBestBid.has_value() && (prevBestBid->price != currentBestBid->price || prevBestBid->quantity != currentBestBid->quantity));

    bool askChanged = (prevBestAsk.has_value() != currentBestAsk.has_value()) || 
                    (currentBestAsk.has_value() && (prevBestAsk->price != currentBestAsk->price || prevBestAsk->quantity != currentBestAsk->quantity));

    if (bidChanged || askChanged) {
        dispatcher.publish(BookUpdateEvent{
            order->getSymbolID(),
            currentBestBid.value_or(MarketData{0, 0}).price,
            currentBestBid.value_or(MarketData{0, 0}).quantity,
            currentBestAsk.value_or(MarketData{0, 0}).price,
            currentBestAsk.value_or(MarketData{0, 0}).quantity
        });
    }

    if (order->getQuantity() > 0) {
        if (order->getTimeInForce() == TimeInForce::IOC || order->getTimeInForce() == TimeInForce::FOK) {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getSymbolID(), order->getOrderID(), order->getTraderID(), order->getQuantity()});
        } else if (order->getOrderType() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release())), *book); 
        } else {
            order->setOrderStatus(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent{order->getSymbolID(), order->getOrderID(), order->getTraderID(), order->getQuantity()});
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
    TraderID traderID;
    Quantity quantity;
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
        Order* order = book->getOrder(orderID);
        if (order) {
            traderID = order->getTraderID();
            quantity = order->getQuantity();
            try {
                book->cancelOrder(orderID);
                dispatcher.publish(OrderCancelledEvent{symbolID, orderID, traderID, quantity});
                std::lock_guard<std::mutex> lock(order_map_mutex_);
                orderID_to_symbol_.erase(orderID);
            } catch (const std::invalid_argument& e) {
                // TODO: add logging
            }
        }
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder, OrderBook& book) {
    if (incomingOrder->getTimeInForce() == TimeInForce::FOK) {
        Quantity availableQuantityAtBestPrice = 0;
        if (incomingOrder->getSide() == Side::BUY) {
            if (book.getBestAsk().has_value()) {
                auto bestAsk = book.getBestAsk().value();
                if (incomingOrder->getOrderType() == OrderType::MARKET || incomingOrder->getPrice() >= bestAsk.price) {
                    availableQuantityAtBestPrice = bestAsk.quantity;
                }
            }
        } else { // incomingOrder->getSide() == Side::SELL
            if (book.getBestBid().has_value()) {
                auto bestBid = book.getBestBid().value();
                if (incomingOrder->getOrderType() == OrderType::MARKET || incomingOrder->getPrice() <= bestBid.price) {
                    availableQuantityAtBestPrice = bestBid.quantity;
                }
            }
        }
        if (incomingOrder->getQuantity() > availableQuantityAtBestPrice) {
            return; // FOK order cannot be fully filled at best price, so it's rejected
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
        
        // Use the new callback-based forEachOrderAtPrice
        bool continueMatchingAtPriceLevel = book.forEachOrderAtPrice(bestOpposingLevel->price,
            incomingOrder->getSide() == Side::BUY ? Side::SELL : Side::BUY,
            [&](OrderID restingOrderID) {
                Order* restingOrder = book.getOrder(restingOrderID); 
                if (!restingOrder) return true; // Continue iteration if order not found

                // Self Match Prevention
                if (restingOrder->getTraderID() == incomingOrder->getTraderID()) {
                    dispatcher.publish(OrderCancelledEvent{
                        restingOrder->getSymbolID(),
                        restingOrderID,
                        restingOrder->getTraderID(),
                        restingOrder->getQuantity()
                    });

                    book.cancelOrder(restingOrderID);
                    return true;
                }

                Quantity tradeQuantity = std::min(incomingOrder->getQuantity(), restingOrder->getQuantity());
                Price tradePrice = restingOrder->getPrice();

                createTrade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

                incomingOrder->setQuantity(incomingOrder->getQuantity() - tradeQuantity);
                book.reduceOrderQuantity(restingOrderID, tradeQuantity); 

                if (incomingOrder->getQuantity() == 0) {
                    return false; // Aggressor fully filled, stop iterating orders at this price level
                }
                return true; // Continue iteration
            });

        // If forEachOrderAtPrice returned false because the incoming order was fully filled,
        // the while loop condition will handle breaking out. No explicit 'break' needed here.
    }
}

void MatchingEngine::placeRestingLimitOrder(std::unique_ptr<LimitOrder> order, OrderBook& book) {
    order->setOrderStatus(OrderStatus::ACCEPTED);
    dispatcher.publish(OrderAcceptedEvent{order->getSymbolID(), order->getOrderID(), order->getTraderID(), order->getSide(), order->getPrice(), order->getQuantity()});
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
