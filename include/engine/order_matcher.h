// include/order_matcher.h
#pragma once
#include "core/order.h"
#include "engine/order_book.h"
#include "engine/event_bus.h"
#include <concurrentqueue.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <variant>
#include <unordered_map>

// Processes every order/cancel on one dedicated worker thread, fed by a
// lock-free moodycamel::ConcurrentQueue so callers on any thread never
// block on submit(). All book mutation happens without locks on this
// one thread; concurrent readers rely on OrderBook's shared_mutex.
class OrderMatcher {
public:
    explicit OrderMatcher(EventBus& bus);
    ~OrderMatcher();

    void start();
    void stop();

    void submit(Order order);
    void cancel(OrderId id);

    const OrderBook& book() const { return book_; }

private:
    struct SubmitCmd { Order order; };
    struct CancelCmd { OrderId id; };
    using Command = std::variant<SubmitCmd, CancelCmd>;

    void run_loop();
    void process_order(const Order& order);
    void process_limit(const LimitOrder& order, std::chrono::steady_clock::time_point dequeue_tp);
    void process_market(const MarketOrder& order, std::chrono::steady_clock::time_point dequeue_tp);
    void process_stop_limit(const StopLimitOrder& order);
    void process_stop_market(const StopMarketOrder& order);
    void try_match_limit(const LimitOrder& taker, std::chrono::steady_clock::time_point dequeue_tp);
    void check_stop_orders(Price last_trade_price);

    EventBus&                                    bus_;
    OrderBook                                    book_;
    moodycamel::ConcurrentQueue<Command>         queue_;
    std::atomic<bool>                            running_{false};
    std::thread                                  worker_;

    // Stop orders waiting to be triggered.
    std::unordered_map<OrderId, StopLimitOrder>  pending_stop_limits_;
    std::unordered_map<OrderId, StopMarketOrder> pending_stop_markets_;

    // Track filled quantities per order for partial fills.
    std::unordered_map<OrderId, Quantity>        filled_qty_;
};
