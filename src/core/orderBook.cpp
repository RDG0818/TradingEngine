// src/core/orderBook.cpp

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "orderBook.h"

void OrderBook::add_order(std::shared_ptr<LimitOrder> order) {
  const Side side = order->get_side();
  const OrderID order_id = order->get_order_id();
  const Price price = order->get_price();
  const Quantity quantity = order->get_quantity();

  if (side == Side::BUY) {
    std::scoped_lock lock(orders_mtx_, bids_mtx_);
    if (all_orders_.count(order_id) > 0) {
      throw std::invalid_argument("Order ID already exists. ID must be unique.");
    }

    PriceLevel& price_level = bids_[price];
    price_level.total_quantity += quantity;
    price_level.orders.push_back(order_id);
    order_iterators_[order_id] = std::prev(price_level.orders.end());
    all_orders_[order_id] = std::move(order);
  } 
  else { // Side::SELL
    std::scoped_lock lock(orders_mtx_, asks_mtx_);
    if (all_orders_.count(order_id) > 0) {
      throw std::invalid_argument("Order ID already exists. ID must be unique.");
    }

    PriceLevel& price_level = asks_[price];
    price_level.total_quantity += quantity;
    price_level.orders.push_back(order_id);
    order_iterators_[order_id] = std::prev(price_level.orders.end());
    all_orders_[order_id] = std::move(order);
  }
}

// TODO: Consider creating a helper function to reduce code duplication
//                    in add_order, remove_order, and reduce_order_quantity,
//                    as they contain similar if/else branching for BUY/SELL sides.
void OrderBook::remove_order(std::unordered_map<OrderID, std::shared_ptr<Order>>::iterator all_orders_it) {
  Order* order = all_orders_it->second.get(); // Note: Locking is handled by the calling function (e.g., cancel_order, reduce_order_quantity)
  const OrderID order_id = order->get_order_id();
  const Price price = order->get_price();
  const Side side = order->get_side();
  const Quantity quantity = order->get_quantity();

  if (side == Side::BUY) {
    auto price_level_it = bids_.find(price);
    if (price_level_it != bids_.end()) {
      PriceLevel& price_level = price_level_it->second;
      price_level.total_quantity -= quantity;
      auto order_it = order_iterators_.find(order_id);
      if (order_it != order_iterators_.end()) {
        price_level.orders.erase(order_it->second);
        order_iterators_.erase(order_it);
      }

      if (price_level.orders.empty()) {
        bids_.erase(price_level_it);
      }
    } 
  } 
  else { // Side::SELL
    auto price_level_it = asks_.find(price);
    if (price_level_it != asks_.end()) {
      PriceLevel& price_level = price_level_it->second;
      price_level.total_quantity -= quantity;

      auto order_it = order_iterators_.find(order_id);
      if (order_it != order_iterators_.end()) {
        price_level.orders.erase(order_it->second);
        order_iterators_.erase(order_it);
      }

      if (price_level.orders.empty()) {
        asks_.erase(price_level_it);
      }
    } 
  }

  all_orders_.erase(all_orders_it);
}

void OrderBook::cancel_order(OrderID order_id) {
  std::unique_lock<std::shared_mutex> orders_lock(orders_mtx_);
  auto all_orders_it = all_orders_.find(order_id);
  if (all_orders_it == all_orders_.end()) {
    return;
  }

  if (all_orders_it->second->get_side() == Side::BUY) {
    std::unique_lock<std::shared_mutex> bids_lock(bids_mtx_);
    remove_order(all_orders_it);
  } 
  else { // Side::SELL
    std::unique_lock<std::shared_mutex> asks_lock(asks_mtx_);
    remove_order(all_orders_it);
  }
}

Order* OrderBook::get_order(OrderID order_id) {
  std::shared_lock<std::shared_mutex> lock(orders_mtx_);
  auto it = all_orders_.find(order_id);
  if (it != all_orders_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void OrderBook::reduce_order_quantity(OrderID order_id, Quantity quantity_to_reduce) {
  std::unique_lock<std::shared_mutex> orders_lock(orders_mtx_);
  auto all_orders_it = all_orders_.find(order_id);
  if (all_orders_it == all_orders_.end()) {
    return;
  }

  Order* order = all_orders_it->second.get();
  const Price price = order->get_price();
  const Side side = order->get_side();

  if (side == Side::BUY) {
    std::unique_lock<std::shared_mutex> bids_lock(bids_mtx_);
    if (quantity_to_reduce > order->get_quantity()) {
      quantity_to_reduce = order->get_quantity();
    }
    order->set_quantity(order->get_quantity() - quantity_to_reduce);
    bids_.at(price).total_quantity -= quantity_to_reduce;
    if (order->get_quantity() == 0) {
      remove_order(all_orders_it);
    }
  } 
  else {
    std::unique_lock<std::shared_mutex> asks_lock(asks_mtx_);
    if (quantity_to_reduce > order->get_quantity()) {
      quantity_to_reduce = order->get_quantity();
    }
    order->set_quantity(order->get_quantity() - quantity_to_reduce);
    asks_.at(price).total_quantity -= quantity_to_reduce;
    if (order->get_quantity() == 0) {
      remove_order(all_orders_it);
    }
  }
}

std::optional<MarketData> OrderBook::get_best_bid() {
  std::shared_lock<std::shared_mutex> lock(bids_mtx_);
  if (bids_.empty()) {
    return std::nullopt;
  }
  const auto& [price, price_level] = *bids_.begin();
  return MarketData{price, price_level.total_quantity};
}

std::optional<MarketData> OrderBook::get_best_ask() {
  std::shared_lock<std::shared_mutex> lock(asks_mtx_);
  if (asks_.empty()) {
    return std::nullopt;
  }
  const auto& [price, price_level] = *asks_.begin();
  return MarketData{price, price_level.total_quantity};
}

bool OrderBook::is_empty() {
  std::shared_lock<std::shared_mutex> lock(orders_mtx_);
  return all_orders_.empty();
}

bool OrderBook::is_side_empty(Side side) {
  if (side == Side::BUY) {
    std::shared_lock<std::shared_mutex> lock(bids_mtx_);
    return bids_.empty();
  } 
  else { // Side::SELL
    std::shared_lock<std::shared_mutex> lock(asks_mtx_);
    return asks_.empty();
  }
}

bool OrderBook::for_each_order_at_price(Price price, Side side, const std::function<bool(OrderID)>& callback) {
  std::list<OrderID> orders_at_price;
  if (side == Side::BUY) { 
    std::shared_lock<std::shared_mutex> lock(bids_mtx_);
    auto bidIt = bids_.find(price);
    if (bidIt != bids_.end()) {
      orders_at_price.assign(bidIt->second.orders.begin(), bidIt->second.orders.end()); // copied by value
    }
  }
  else { // Side::SELL 
    std::shared_lock<std::shared_mutex> lock(asks_mtx_);
    auto askIt = asks_.find(price);
    if (askIt != asks_.end()) {
      orders_at_price.assign(askIt->second.orders.begin(), askIt->second.orders.end()); // copied by value
    }
  }

  for (const auto& order_id : orders_at_price) {
    if (!callback(order_id)) {
      return false; 
    }
  }
  return true; 
}

void OrderBook::print(int num_price_levels) const {
  std::cout << std::endl;
  const std::string red_color = "\033[31m";
  const std::string green_color = "\033[32m";
  const std::string reset_color = "\033[0m";

  struct PrintLevel {
    Price price;
    Quantity quantity;
    Side side;
  };

  std::vector<PrintLevel> display_levels;
  Quantity max_quantity_in_display = 0; // Max quantity among displayed levels
  std::optional<Price> best_ask_price;
  std::optional<Price> best_bid_price;

  // Collect asks for display, limited by priceLevels
  {
    std::shared_lock lock(asks_mtx_);
    if (!asks_.empty()) {
      best_ask_price = asks_.begin()->first;
    }
    auto it = asks_.begin();
    for (int i = 0; i < num_price_levels && it != asks_.end(); ++i, ++it) {
      display_levels.push_back({it->first, it->second.total_quantity, Side::SELL});
      if (it->second.total_quantity > max_quantity_in_display) {
        max_quantity_in_display = it->second.total_quantity;
      }
    }
  }

  // Collect bids for display, limited by priceLevels
  {
    std::shared_lock lock(bids_mtx_);
    if (!bids_.empty()) {
      best_bid_price = bids_.begin()->first;
    }
    auto it = bids_.begin();
    for (int i = 0; i < num_price_levels && it != bids_.end(); ++i, ++it) {
      display_levels.push_back({it->first, it->second.total_quantity, Side::BUY});
      if (it->second.total_quantity > max_quantity_in_display) {
        max_quantity_in_display = it->second.total_quantity;
      }
    }
  }

  // Sort the collected levels by price descending for display
  std::sort(display_levels.begin(), display_levels.end(), [](const PrintLevel& a, const PrintLevel& b) {
    return a.price > b.price;
  });

  std::cout << std::left << std::setw(10) << "Price"
            << std::right << std::setw(10) << "Quantity"
            << "   Volume" << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  const int bar_width = 50;
  bool spread_printed = false;

  for (const auto& level : display_levels) {
    // Check if we should print the spread.
    // This condition triggers when we transition from asks to bids in the sorted display.
    if (!spread_printed && best_bid_price.has_value() && level.side == Side::BUY) {
      // Find the highest priced ask among the displayed asks to determine the actual visual spread point
      // This is necessary because display_levels is sorted descending by price,
      // so the first bid we encounter after potentially some asks indicates the spread position.
      std::optional<Price> lowest_displayed_ask_price;
      for (const auto& l : display_levels) {
        if (l.side == Side::SELL) {
          lowest_displayed_ask_price = l.price;
        } 
        else {
          break; // stop when we hit bids
        }
      }

      spread_printed = true;
    }

    const std::string& color = (level.side == Side::BUY) ? green_color : red_color;
    double price = static_cast<double>(level.price) / 10000.0;

    int bar_length = 0;
    if (max_quantity_in_display > 0) {
      bar_length = static_cast<int>((static_cast<double>(level.quantity) / max_quantity_in_display) * bar_width);
    }
    if (level.quantity > 0 && bar_length == 0 && max_quantity_in_display > 0) {
      bar_length = 1; 
    }
    std::string bar(bar_length, '='); 

    std::cout << color
              << std::left << std::setw(10) << std::fixed << std::setprecision(4) << price
              << std::right << std::setw(10) << level.quantity << " "
              << std::left << bar
              << reset_color << std::endl;
  }

  std::cout << std::endl;

}