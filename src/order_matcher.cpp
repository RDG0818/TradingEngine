// src/order_matcher.cpp
#include "order_matcher.h"
#include "exchange_events.h"
#include <chrono>

OrderMatcher::OrderMatcher(EventBus& bus) : bus_(bus) {}

OrderMatcher::~OrderMatcher() { stop(); }

void OrderMatcher::start() {
    running_ = true;
    worker_ = std::thread(&OrderMatcher::run_loop, this);
}

void OrderMatcher::stop() {
    if (running_.exchange(false))
        worker_.join();
}

void OrderMatcher::submit(Order order) {
    queue_.enqueue(SubmitCmd{std::move(order)});
}

void OrderMatcher::cancel(OrderId id) {
    queue_.enqueue(CancelCmd{id});
}

void OrderMatcher::run_loop() {
    Command cmd;
    while (running_) {
        while (queue_.try_dequeue(cmd)) {
            std::visit([this](auto& c) {
                if constexpr (std::is_same_v<std::decay_t<decltype(c)>, SubmitCmd>) {
                    process_order(c.order);
                } else {
                    // Look up trader_id before cancelling.
                    TraderId tid = 0;
                    const LimitOrder* lo = book_.find_order(c.id);
                    if (lo) tid = lo->trader_id;
                    if (book_.cancel_order(c.id)) {
                        bus_.publish(OrderCancelledEvent{c.id, tid});
                        bus_.publish(BookUpdateEvent{book_.snapshot()});
                    }
                }
            }, cmd);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void OrderMatcher::process_order(const Order& order) {
    auto dequeue_tp = std::chrono::steady_clock::now();
    std::visit([this, dequeue_tp](const auto& o) {
        using T = std::decay_t<decltype(o)>;
        if      constexpr (std::is_same_v<T, LimitOrder>)      process_limit(o, dequeue_tp);
        else if constexpr (std::is_same_v<T, MarketOrder>)     process_market(o, dequeue_tp);
        else if constexpr (std::is_same_v<T, StopLimitOrder>)  process_stop_limit(o);
        else if constexpr (std::is_same_v<T, StopMarketOrder>) process_stop_market(o);
    }, order);
}

void OrderMatcher::process_limit(const LimitOrder& order, std::chrono::steady_clock::time_point dequeue_tp) {
    try_match_limit(order, dequeue_tp);
}

void OrderMatcher::try_match_limit(const LimitOrder& taker, std::chrono::steady_clock::time_point dequeue_tp) {
    Quantity remaining = taker.qty;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    Price last_price = 0;

    auto walk_cb = [&](Price level_price, Quantity, const std::vector<OrderId>& ids) -> bool {
        bool price_ok = (taker.side == Side::Buy)
            ? (level_price <= taker.price)
            : (level_price >= taker.price);
        if (!price_ok || remaining == 0) return true; // stop

        for (OrderId maker_id : ids) {
            if (remaining == 0) break;
            const LimitOrder* maker = book_.find_order(maker_id);
            if (!maker) continue;
            if (maker->trader_id == taker.trader_id) continue; // self-match prevention

            Quantity maker_remaining = maker->qty - filled_qty_[maker_id];
            Quantity fill_qty = std::min(remaining, maker_remaining);

            Fill fill{maker_id, taker.id, maker->trader_id, taker.trader_id,
                      level_price, fill_qty, now, taker.side};
            bus_.publish(FillEvent{fill});

            filled_qty_[maker_id] += fill_qty;
            remaining             -= fill_qty;
            last_price             = level_price;

            if (filled_qty_[maker_id] >= maker->qty) {
                book_.cancel_order(maker_id);
                filled_qty_.erase(maker_id);
            }
        }
        return remaining == 0;
    };

    if (taker.side == Side::Buy)
        book_.for_each_ask(walk_cb);
    else
        book_.for_each_bid(walk_cb);

    if (remaining > 0 && taker.tif == TimeInForce::GTC) {
        // Rest the unfilled portion in the book with adjusted qty.
        LimitOrder resting = taker;
        resting.qty = remaining;
        book_.add_order(resting);
        bus_.publish(OrderAcceptedEvent{taker.id, taker.trader_id, dequeue_tp});
        bus_.publish(BookUpdateEvent{book_.snapshot()});
    } else if (remaining > 0 && taker.tif != TimeInForce::GTC) {
        // IOC/FOK: remaining portion is not rested.
        if (last_price == 0) {
            // Nothing filled at all — reject.
            bus_.publish(OrderRejectedEvent{taker.id, taker.trader_id, "no_liquidity"});
        }
        // Partially filled IOC: filled portion already emitted FillEvents above.
        // Emit a book update if the book changed (makers were removed).
        if (last_price != 0) {
            bus_.publish(BookUpdateEvent{book_.snapshot()});
        }
    } else if (remaining == 0) {
        // Fully matched immediately.
        bus_.publish(BookUpdateEvent{book_.snapshot()});
    }

    if (last_price > 0) {
        check_stop_orders(last_price);
    }
}

void OrderMatcher::process_market(const MarketOrder& order, std::chrono::steady_clock::time_point dequeue_tp) {
    // Convert to aggressive limit at extreme price, IOC semantics.
    Price aggressive_price = (order.side == Side::Buy) ? UINT64_MAX : 0;
    LimitOrder synthetic{order.id, order.trader_id, order.side,
                         aggressive_price, order.qty, TimeInForce::IOC, order.ts};
    try_match_limit(synthetic, dequeue_tp);
}

void OrderMatcher::process_stop_limit(const StopLimitOrder& order) {
    pending_stop_limits_.emplace(order.id, order);
}

void OrderMatcher::process_stop_market(const StopMarketOrder& order) {
    pending_stop_markets_.emplace(order.id, order);
}

void OrderMatcher::check_stop_orders(Price last_price) {
    std::vector<OrderId> to_trigger;

    for (const auto& [id, o] : pending_stop_limits_) {
        bool triggered = (o.side == Side::Buy)  ? (last_price >= o.stop_price)
                                                 : (last_price <= o.stop_price);
        if (triggered) to_trigger.push_back(id);
    }
    for (OrderId id : to_trigger) {
        auto& o = pending_stop_limits_.at(id);
        LimitOrder triggered{o.id, o.trader_id, o.side, o.limit_price, o.qty,
                             TimeInForce::GTC, o.ts};
        process_limit(triggered, std::chrono::steady_clock::now());
        pending_stop_limits_.erase(id);
    }
    to_trigger.clear();

    for (const auto& [id, o] : pending_stop_markets_) {
        bool triggered = (o.side == Side::Buy)  ? (last_price >= o.stop_price)
                                                 : (last_price <= o.stop_price);
        if (triggered) to_trigger.push_back(id);
    }
    for (OrderId id : to_trigger) {
        auto& o = pending_stop_markets_.at(id);
        MarketOrder triggered{o.id, o.trader_id, o.side, o.qty, TimeInForce::IOC, o.ts};
        process_market(triggered, std::chrono::steady_clock::now());
        pending_stop_markets_.erase(id);
    }
}
