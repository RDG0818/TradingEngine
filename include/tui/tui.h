#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>
#include "engine/exchange.h"
#include "trader_registry.h"
#include "engine/stats_tracker.h"
#include "core/order.h"

namespace ftxui { class ScreenInteractive; }

class TUI {
public:
    TUI(Exchange& exchange, TraderRegistry& registry);
    ~TUI();

    // Blocks until the user quits (q or Ctrl-C)
    void run();

private:
    void on_book_update(const BookSnapshot& snap);
    void on_fill(const Fill& fill);

    void handle_command(const std::string& cmd);
    void handle_order_command(const std::string& cmd);
    void handle_slash_command(const std::string& cmd);

    Exchange& exchange_;
    TraderRegistry& registry_;

    mutable std::mutex state_mutex_;
    BookSnapshot book_;
    std::deque<Fill> recent_fills_;
    std::deque<Fill> user_fills_;
    std::string status_message_;

    TraderId user_id_{0};
    std::unordered_set<Price> user_bid_prices_;
    std::unordered_set<Price> user_ask_prices_;

    StatsTracker stats_;
    std::string command_input_;

    ftxui::ScreenInteractive* screen_{nullptr};
    std::vector<SubscriptionToken> tokens_;
};
