#include "matchingEngine.h"
#include "orderFactory.h"
#include "symbolRegistry.h"
#include <variant>
#include <iostream>

MatchingEngine::MatchingEngine(EventDispatcher& eventDispatcher)
    : dispatcher(eventDispatcher), nextOrderID(1) {}

MatchingEngine::~MatchingEngine() {
    stop();
}

OrderID MatchingEngine::submitOrder(const RawOrderParams& params) {
    OrderID id = nextOrderID.fetch_add(1);
    
    try {
        SymbolID symbolID = SymbolRegistry::get_instance().get_id(params.symbol);
        {
            std::lock_guard<std::mutex> lock(order_map_mutex_);
            orderID_to_symbol_[id] = symbolID;
        }

        auto order = order_factory::create_order(params, id);
        event_queue.push(std::move(order));
    } catch (const InvalidPriceException& e) {
        dispatcher.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_PRICE, e.what()));
    } catch (const InvalidQuantityException& e) {
        dispatcher.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::INVALID_QUANTITY, e.what()));
    } catch (const UnsupportedOrderTypeException& e) {
        dispatcher.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::UNSUPPORTED_ORDER_TYPE, e.what()));
    } catch (const std::invalid_argument& e) {
        dispatcher.publish(OrderRejectedEvent(id, params.trader_id, RejectionReason::OTHER, e.what()));
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
    if (order->get_quantity() == 0) {
        dispatcher.publish(OrderRejectedEvent(order->get_order_id(), order->get_trader_id(), RejectionReason::INVALID_QUANTITY, "Quantity must be positive."));
        return;
    }

    if (order->get_order_type() == OrderType::STOP_MARKET) {
        processStopMarketOrder(std::unique_ptr<StopMarketOrder>(static_cast<StopMarketOrder*>(order.release())));
        return;
    } else if (order->get_order_type() == OrderType::STOP_LIMIT) {
        processStopLimitOrder(std::unique_ptr<StopLimitOrder>(static_cast<StopLimitOrder*>(order.release())));
        return;
    }

    OrderBook* book = getOrCreateOrderBook(order->get_symbol_id());

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
        dispatcher.publish(BookUpdateEvent(
            order->get_symbol_id(),
            currentBestBid.value_or(MarketData{0, 0}).price,
            currentBestBid.value_or(MarketData{0, 0}).quantity,
            currentBestAsk.value_or(MarketData{0, 0}).price,
            currentBestAsk.value_or(MarketData{0, 0}).quantity
        ));
    }

    if (order->get_quantity() > 0) {
        if (order->get_time_in_force() == TimeInForce::IOC || order->get_time_in_force() == TimeInForce::FOK) {
            order->set_order_status(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
        } else if (order->get_order_type() == OrderType::LIMIT) {
           placeRestingLimitOrder(std::unique_ptr<LimitOrder>(static_cast<LimitOrder*>(order.release())), *book); 
        } else {
            order->set_order_status(OrderStatus::CANCELLED);
            dispatcher.publish(OrderCancelledEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_quantity()));
        }
    }
}

void MatchingEngine::processStopMarketOrder(std::unique_ptr<StopMarketOrder> order) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->get_side() == Side::BUY) {
        stop_buy_orders_[order->get_stop_price()].push_back(order.release());
    } else {
        stop_sell_orders_[order->get_stop_price()].push_back(order.release());
    }
}

void MatchingEngine::processStopLimitOrder(std::unique_ptr<StopLimitOrder> order) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);
    if (order->get_side() == Side::BUY) {
        stop_buy_orders_[order->get_stop_price()].push_back(order.release());
    } else {
        stop_sell_orders_[order->get_stop_price()].push_back(order.release());
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
            traderID = order->get_trader_id();
            quantity = order->get_quantity();
            try {
                book->cancelOrder(orderID);
                dispatcher.publish(OrderCancelledEvent(symbolID, orderID, traderID, quantity));
                std::lock_guard<std::mutex> lock(order_map_mutex_);
                orderID_to_symbol_.erase(orderID);
            } catch (const std::invalid_argument& e) {
                // TODO: add logging
            }
        }
    }
}

void MatchingEngine::matchOrder(Order* incomingOrder, OrderBook& book) {
    if (incomingOrder->get_time_in_force() == TimeInForce::FOK) {
        Quantity availableQuantityAtBestPrice = 0;
        if (incomingOrder->get_side() == Side::BUY) {
            if (book.getBestAsk().has_value()) {
                auto bestAsk = book.getBestAsk().value();
                if (incomingOrder->get_order_type() == OrderType::MARKET || incomingOrder->get_price() >= bestAsk.price) {
                    availableQuantityAtBestPrice = bestAsk.quantity;
                }
            }
        } else { // incomingOrder->get_side() == Side::SELL
            if (book.getBestBid().has_value()) {
                auto bestBid = book.getBestBid().value();
                if (incomingOrder->get_order_type() == OrderType::MARKET || incomingOrder->get_price() <= bestBid.price) {
                    availableQuantityAtBestPrice = bestBid.quantity;
                }
            }
        }
        if (incomingOrder->get_quantity() > availableQuantityAtBestPrice) {
            return; // FOK order cannot be fully filled at best price, so it's rejected
        }
    }

    while (incomingOrder->get_quantity() > 0) {
        std::optional<MarketData> bestOpposingLevel = (incomingOrder->get_side() == Side::BUY) ? book.getBestAsk() : book.getBestBid();
        
        if (!bestOpposingLevel) break;

        if (incomingOrder->get_order_type() == OrderType::LIMIT) {
            auto limitOrder = static_cast<LimitOrder*>(incomingOrder);
            Price limitPrice = limitOrder->get_price();
            Price bestOpposingPrice = bestOpposingLevel->price;

            if (incomingOrder->get_side() == Side::BUY && limitPrice < bestOpposingPrice) break;
            if (incomingOrder->get_side() == Side::SELL && limitPrice > bestOpposingPrice) break;
        }
        
        // Use the new callback-based forEachOrderAtPrice
        bool continueMatchingAtPriceLevel = book.forEachOrderAtPrice(bestOpposingLevel->price,
            incomingOrder->get_side() == Side::BUY ? Side::SELL : Side::BUY,
            [&](OrderID restingOrderID) {
                Order* restingOrder = book.getOrder(restingOrderID); 
                if (!restingOrder) return true; // Continue iteration if order not found

                // Self Match Prevention
                if (restingOrder->get_trader_id() == incomingOrder->get_trader_id()) {
                    dispatcher.publish(OrderCancelledEvent(
                        restingOrder->get_symbol_id(),
                        restingOrderID,
                        restingOrder->get_trader_id(),
                        restingOrder->get_quantity()
                    ));

                    book.cancelOrder(restingOrderID);
                    return true;
                }

                Quantity tradeQuantity = std::min(incomingOrder->get_quantity(), restingOrder->get_quantity());
                Price tradePrice = restingOrder->get_price();

                createTrade(incomingOrder, restingOrder, tradePrice, tradeQuantity);

                incomingOrder->set_quantity(incomingOrder->get_quantity() - tradeQuantity);
                book.reduceOrderQuantity(restingOrderID, tradeQuantity); 

                if (incomingOrder->get_quantity() == 0) {
                    return false; // Aggressor fully filled, stop iterating orders at this price level
                }
                return true; // Continue iteration
            });

        // If forEachOrderAtPrice returned false because the incoming order was fully filled,
        // the while loop condition will handle breaking out. No explicit 'break' needed here.
    }
}

void MatchingEngine::placeRestingLimitOrder(std::unique_ptr<LimitOrder> order, OrderBook& book) {
    order->set_order_status(OrderStatus::ACCEPTED);
    dispatcher.publish(OrderAcceptedEvent(order->get_symbol_id(), order->get_order_id(), order->get_trader_id(), order->get_side(), order->get_price(), order->get_quantity()));
    book.addOrder(std::move(order));
}

void MatchingEngine::createTrade(Order* aggressor, Order* resting, Price tradePrice, Quantity tradeQuantity) {

    Quantity aggressorRemaining = aggressor->get_quantity() - tradeQuantity;
    Quantity restingRemaining = resting->get_quantity() - tradeQuantity;
    aggressor->set_order_status(aggressorRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    resting->set_order_status(restingRemaining > 0 ? OrderStatus::PARTIALLY_FILLED : OrderStatus::FILLED);
    
    dispatcher.publish(TradeExecutedEvent(aggressor->get_symbol_id(), tradePrice, tradeQuantity, 
        aggressor->get_order_id(), aggressor->get_trader_id(), aggressor->get_side(), aggressor->get_quantity(), 
        resting->get_order_id(), resting->get_trader_id(), resting->get_quantity()));

    if (aggressorRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(aggressor->get_order_id());
    }
    if (restingRemaining == 0) {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        orderID_to_symbol_.erase(resting->get_order_id());
    }

    triggerStopOrders(tradePrice);
}

void MatchingEngine::triggerStopOrders(Price lastTradePrice) {
    std::lock_guard<std::mutex> lock(stop_orders_mutex_);

    // Trigger buy stop orders
    for (auto it = stop_buy_orders_.begin(); it != stop_buy_orders_.end() && it->first <= lastTradePrice; ) {
        for (auto& order : it->second) {
            if (order->get_order_type() == OrderType::STOP_MARKET) {
                event_queue.push(std::make_unique<MarketOrder>(order->get_symbol_id(), order->get_order_id(), order->get_side(), order->get_quantity(), order->get_trader_id()));
            } else if (order->get_order_type() == OrderType::STOP_LIMIT) {
                event_queue.push(std::make_unique<LimitOrder>(order->get_symbol_id(), order->get_order_id(), order->get_side(), order->get_price(), order->get_quantity(), order->get_trader_id()));
            }
        }
        it = stop_buy_orders_.erase(it);

    }

    // Trigger sell stop orders

    for (auto it = stop_sell_orders_.begin(); it != stop_sell_orders_.end() && it->first >= lastTradePrice; ) {
        for (auto& order : it->second) {
            if (order->get_order_type() == OrderType::STOP_MARKET) {
                event_queue.push(std::make_unique<MarketOrder>(order->get_symbol_id(), order->get_order_id(), order->get_side(), order->get_quantity(), order->get_trader_id()));
            } else if (order->get_order_type() == OrderType::STOP_LIMIT) {
                event_queue.push(std::make_unique<LimitOrder>(order->get_symbol_id(), order->get_order_id(), order->get_side(), order->get_price(), order->get_quantity(), order->get_trader_id()));
            }
        }
        it = stop_sell_orders_.erase(it);
    }
}

std::optional<MarketData> MatchingEngine::getBestAsk(SymbolID symbolID) const {
    std::lock_guard<std::mutex> lock(books_mutex_);
    auto it = books_.find(symbolID);
    if (it != books_.end()) {
        return it->second->getBestAsk();
    }
    return std::nullopt;
}

std::optional<MarketData> MatchingEngine::getBestBid(SymbolID symbolID) const {
    std::lock_guard<std::mutex> lock(books_mutex_);
    auto it = books_.find(symbolID);
    if (it != books_.end()) {
        return it->second->getBestBid();
    }
    return std::nullopt;
}

void MatchingEngine::printTopOfBook(std::string symbol, int numPriceLevels) {
    SymbolID symbolID = SymbolRegistry::get_instance().get_id(symbol);
    OrderBook* ob = getBook(symbolID);
    std::cout << std::endl << "\x1b[1m" << "\033[1;33m" << symbol << "\033[0m" 
    << " Top of Orderbook: " << "\x1b[0m" << std::endl;
    if (ob != nullptr) ob->print(numPriceLevels);
}
