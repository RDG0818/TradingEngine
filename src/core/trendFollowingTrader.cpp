#include "trading_engine/trendFollowingTrader.h"
#include "trading_engine/events.h"
#include <iostream>

TrendFollowingTrader::TrendFollowingTrader(MatchingEngine& engine, EventDispatcher& dispatcher, const std::string& symbol, std::chrono::milliseconds interval, size_t trend_window)
    : Trader(engine, dispatcher),
      symbol_id_(SymbolRegistry::getInstance().getID(symbol)),
      interval_(interval),
      trend_window_(trend_window) {
    dispatcher_.subscribe(EventType::TRADE_EXECUTED, this);
    last_tick_ = std::chrono::steady_clock::now();
}

void TrendFollowingTrader::onEvent(const Event& event) {
    if (event.type == EventType::TRADE_EXECUTED) {
        const auto& trade_event = static_cast<const TradeExecutedEvent&>(event);
        if (trade_event.symbolID == symbol_id_) {
            std::lock_guard<std::mutex> lock(mtx_);
            price_history_.push_back(trade_event.tradePrice);
            if (price_history_.size() > trend_window_) {
                price_history_.pop_front();
            }
        }
    }
}

void TrendFollowingTrader::tick() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_tick_ < interval_) {
        return;
    }
    last_tick_ = now;

    std::vector<Price> prices;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (price_history_.size() < trend_window_) {
            return;
        }
        prices.assign(price_history_.begin(), price_history_.end());
    }

    // Simple trend detection: check if prices are consistently increasing or decreasing
    bool uptrend = true;
    bool downtrend = true;
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i] < prices[i-1]) uptrend = false;
        if (prices[i] > prices[i-1]) downtrend = false;
    }

    if (uptrend) {
        RawOrderParams params = {
            .symbol = SymbolRegistry::getInstance().getSymbol(symbol_id_),
            .traderID = 200, // Example trader ID
            .side = Side::BUY,
            .orderType = OrderType::LIMIT,
            .quantity = 5,
            .price = 200
        };
        try {
            OrderID orderID = engine_.submitOrder(params);
        } catch (const std::exception& e) {
            std::cerr << "TrendFollowingTrader failed to submit order: " << e.what() << std::endl;
        }
    } else if (downtrend) {
        RawOrderParams params = {
            .symbol = SymbolRegistry::getInstance().getSymbol(symbol_id_),
            .traderID = 200, // Example trader ID
            .side = Side::SELL,
            .orderType = OrderType::LIMIT,
            .quantity = 5,
            .price = 1
        };
        try {
            OrderID orderID = engine_.submitOrder(params);
        } catch (const std::exception& e) {
            std::cerr << "TrendFollowingTrader failed to submit order: " << e.what() << std::endl;
        }
    }
}
