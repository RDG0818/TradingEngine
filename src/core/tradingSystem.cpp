// src/core/tradingSystem.cpp

#include "tradingSystem.h"

#include <stdexcept>
#include <cstdio>
#include <chrono>
#include <thread>

static constexpr double LATENCY_SMOOTHING_FACTOR = 0.05; // Alpha for EMA
static constexpr size_t MAX_RECENT_TRADES = 100;

TradingSystem::TradingSystem(int tick_interval_ms, const std::vector<std::string>& symbols)
  : dispatcher_(),
    id_generator_(),
    engine_(dispatcher_, id_generator_),
    symbols_(symbols),
    manager_(engine_, dispatcher_, std::chrono::milliseconds(tick_interval_ms), symbols_)
{
    // Subscribe to events to update system-wide state
    dispatcher_.subscribe<TradeExecutedEvent>([this](const TradeExecutedEvent& event) {
        this->on_trade_executed(event);
    });
    dispatcher_.subscribe<BookUpdateEvent>([this](const BookUpdateEvent& event) {
        this->on_book_update(event);
    });
    dispatcher_.subscribe<OrderAcceptedEvent>([this](const OrderAcceptedEvent& event) {
        this->on_order_accepted(event);
    });
    dispatcher_.subscribe<OrderFilledEvent>([this](const OrderFilledEvent& event) {
        this->on_order_filled(event);
    });
    dispatcher_.subscribe<OrderCancelledEvent>([this](const OrderCancelledEvent& event) {
        this->on_order_cancelled(event);
    });
}

TradingSystem::~TradingSystem() {
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void TradingSystem::start() {
    engine_.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager_.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void TradingSystem::stop() {
    manager_.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    engine_.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

bool TradingSystem::is_running() const {
    return engine_.is_running();
}

void TradingSystem::enable_automated_traders(bool enable) {
    if (enable) {
        manager_.start();
    } else {
        manager_.stop();
    }
}

bool TradingSystem::are_automated_traders_enabled() const {
    return manager_.is_running();
}

SystemMetrics TradingSystem::get_system_metrics() const {
    std::lock_guard<std::mutex> lock(system_mutex_);
    SystemMetrics current_metrics = metrics_;
    current_metrics.avg_latency_ms = avg_latency_ns_ / 1e6; // Convert ns to ms
    return current_metrics;
}

std::optional<MarketSnapshot> TradingSystem::get_market_snapshot(const std::string& symbol) const {
    SymbolID symbol_id = SymbolRegistry::get_instance().get_id(symbol);
    std::lock_guard<std::mutex> lock(system_mutex_);
    if (market_snapshots_.count(symbol_id)) {
        return market_snapshots_.at(symbol_id);
    }
    return std::nullopt;
}

std::optional<PortfolioSnapshot> TradingSystem::get_portfolio_snapshot(TraderID trader_id) const {
    std::lock_guard<std::mutex> lock(system_mutex_);

    auto portfolio_it = portfolios_.find(trader_id);
    if (portfolio_it == portfolios_.end()) {
        return std::nullopt;
    }

    const auto& portfolio = portfolio_it->second;

    PortfolioSnapshot snapshot;
    snapshot.balance = portfolio.get_balance();
    snapshot.positions = portfolio.get_all_positions();
    snapshot.trade_history = portfolio.get_trade_history();

    return snapshot;
}

const std::vector<std::string>& TradingSystem::get_all_symbols() const {
    return symbols_;
}

TraderID TradingSystem::create_portfolio(Price starting_balance) {
    TraderID new_id = next_trader_id_++;
    portfolios_.emplace(std::piecewise_construct,
                        std::forward_as_tuple(new_id),
                        std::forward_as_tuple(engine_, starting_balance, new_id));
    return new_id;
}

OrderID TradingSystem::submit_order(TraderID trader_id, const RawOrderParams& params) {
    // Automated traders (ID >= 100000) submit directly to the engine
    // This preserves their behavior without portfolio checks
    if (trader_id >= 100000) {
        return engine_.submit_order(params);
    }

    auto portfolio_it = portfolios_.find(trader_id);
    if (portfolio_it == portfolios_.end()) {
        // To publish a rejection, we need an order ID, but we can't create an order
        // without a valid trader. We'll throw an exception as the calling client
        // provided an invalid trader ID.
        throw std::invalid_argument("Portfolio for trader ID does not exist.");
    }
    auto& portfolio = portfolio_it->second;

    OrderID new_order_id = id_generator_.new_id();

    std::shared_ptr<Order> order;
    try {
        order = order_factory::create_order(params, new_order_id);
    } catch (const InvalidPriceException& e) {
        dispatcher_.publish(OrderRejectedEvent(new_order_id, trader_id, RejectionReason::INVALID_PRICE, e.what()));
        return new_order_id;
    } catch (const InvalidQuantityException& e) {
        dispatcher_.publish(OrderRejectedEvent(new_order_id, trader_id, RejectionReason::INVALID_QUANTITY, e.what()));
        return new_order_id;
    } catch (const UnsupportedOrderTypeException& e) {
        dispatcher_.publish(OrderRejectedEvent(new_order_id, trader_id, RejectionReason::UNSUPPORTED_ORDER_TYPE, e.what()));
        return new_order_id;
    } catch (const std::invalid_argument& e) {
        dispatcher_.publish(OrderRejectedEvent(new_order_id, trader_id, RejectionReason::OTHER, e.what()));
        return new_order_id;
    }

    if (!portfolio.can_submit_order(order)) {
        dispatcher_.publish(OrderRejectedEvent(new_order_id, trader_id, RejectionReason::INSUFFICIENT_FUNDS, "Insufficient funds or position."));
        return new_order_id;
    }

    {
        std::lock_guard<std::mutex> lock(system_mutex_);
        live_orders_[new_order_id] = order;
    }

    engine_.submit_order(order);
    return new_order_id;
}

void TradingSystem::cancel_order(OrderID order_id) {
    engine_.cancel_order(order_id);
}

void TradingSystem::on_trade_executed(const TradeExecutedEvent& event) {
    std::lock_guard<std::mutex> lock(system_mutex_);

    metrics_.orders_processed++; // A trade implies at least one order was processed

    auto& snapshot = market_snapshots_[event.symbol_id];
    snapshot.last_trade_price = event.price;
    snapshot.last_trade_quantity = event.quantity;

    // Add to recent trades and trim if necessary
    snapshot.recent_trades.push_front(event.price);
    if (snapshot.recent_trades.size() > MAX_RECENT_TRADES) {
        snapshot.recent_trades.pop_back();
    }
    
    // Find and update aggressor's portfolio
    auto aggressor_it = portfolios_.find(event.aggressing_trader_id);
    if (aggressor_it != portfolios_.end()) {
        aggressor_it->second.on_trade_executed(event, live_orders_[event.aggressing_order_id]);
    }

    // Find and update resting party's portfolio
    auto resting_it = portfolios_.find(event.resting_trader_id);
    if (resting_it != portfolios_.end()) {
        resting_it->second.on_trade_executed(event, live_orders_[event.resting_order_id]);
    }
}

void TradingSystem::on_book_update(const BookUpdateEvent& event) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    auto& snapshot = market_snapshots_[event.symbol_id];
    snapshot.best_bid = event.best_bid_price;
    snapshot.best_bid_quantity = event.best_bid_quantity;
    snapshot.best_ask = event.best_ask_price;
    snapshot.best_ask_quantity = event.best_ask_quantity;
}

void TradingSystem::on_order_accepted(const OrderAcceptedEvent& event) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    metrics_.active_orders++;

    // Latency calculation
    auto it = live_orders_.find(event.order_id);
    if (it != live_orders_.end()) {
        auto order_creation_time = it->second->get_timestamp();
        auto event_time = event.get_timestamp();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(event_time - order_creation_time).count();
        
        if (avg_latency_ns_ == 0.0) {
            avg_latency_ns_ = static_cast<double>(latency);
        } else {
            avg_latency_ns_ = (static_cast<double>(latency) * LATENCY_SMOOTHING_FACTOR) + (avg_latency_ns_ * (1.0 - LATENCY_SMOOTHING_FACTOR));
        }
    }
}

void TradingSystem::on_order_filled(const OrderFilledEvent& event) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    if (live_orders_.erase(event.order_id) > 0) { // erase returns number of elements removed
        metrics_.active_orders--;
    }
}

void TradingSystem::on_order_cancelled(const OrderCancelledEvent& event) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    if (live_orders_.erase(event.order_id) > 0) { // erase returns number of elements removed
        metrics_.active_orders--;
    }
}
