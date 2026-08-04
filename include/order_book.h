// include/order_book.h
#pragma once
#include "core/order.h"
#include <boost/container/flat_map.hpp>
#include <functional>
#include <list>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

struct PriceLevel {
    Quantity              total_qty{0};
    std::list<OrderId>    order_ids;
};

struct BookSnapshot {
    std::vector<std::pair<Price, Quantity>> bids;  // descending by price
    std::vector<std::pair<Price, Quantity>> asks;  // ascending by price
};

// Callback for for_each_bid / for_each_ask.
// Receives (price, total_qty_at_level, order_ids_at_level).
// Return true to stop walking, false to continue.
using BookWalkCallback = std::function<bool(Price, Quantity, const std::vector<OrderId>&)>;

class OrderBook {
public:
    void add_order(const LimitOrder& order);
    bool cancel_order(OrderId id);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    // Walk from best price inward. Callback returns true to stop.
    void for_each_bid(BookWalkCallback cb);
    void for_each_ask(BookWalkCallback cb);

    // Look up a resting order (returns nullptr if not found).
    const LimitOrder* find_order(OrderId id) const;

    BookSnapshot snapshot() const;

private:
    // Both sorted ascending; bids accessed rbegin→rend, asks begin→end.
    boost::container::flat_map<Price, PriceLevel> bids_;
    boost::container::flat_map<Price, PriceLevel> asks_;
    std::unordered_map<OrderId, LimitOrder>        orders_;
    mutable std::shared_mutex                      mutex_;
};
