// src/exchange.cpp
#include "exchange.h"
#include "exchange_events.h"

Exchange::Exchange()
    : matcher_(bus_), registry_(matcher_) {}

Exchange::~Exchange() { stop(); }

void Exchange::start(Price seed_price) {
    seed_price_       = seed_price;
    last_trade_price_ = seed_price;

    bus_.subscribe<FillEvent>([this](const FillEvent& e) { on_fill(e); });
    registry_.subscribe_to_fills(bus_);

    matcher_.start();
    registry_.start();
    running_ = true;
}

void Exchange::stop() {
    if (!running_.exchange(false)) return;
    registry_.stop();
    matcher_.stop();
}

TraderId Exchange::create_portfolio(uint64_t balance) {
    TraderId id = next_portfolio_id_.fetch_add(1);
    std::lock_guard lock(portfolios_mutex_);
    portfolios_.emplace(std::piecewise_construct,
                        std::forward_as_tuple(id),
                        std::forward_as_tuple(balance));
    return id;
}

PortfolioSnapshot Exchange::portfolio_snapshot(TraderId id) const {
    std::lock_guard lock(portfolios_mutex_);
    auto it = portfolios_.find(id);
    if (it == portfolios_.end()) return {};
    const Portfolio& p = it->second;
    return {p.balance(), p.position(), p.unrealized_pnl(last_trade_price_), p.avg_cost()};
}

void Exchange::submit_order(Order order) {
    ++orders_processed_;
    matcher_.submit(std::move(order));
}

bool Exchange::cancel_order(OrderId id) {
    matcher_.cancel(id);
    return true; // cancellation is async; result published via event
}

BookSnapshot Exchange::book_snapshot() const {
    return matcher_.book().snapshot();
}

std::vector<Fill> Exchange::recent_trades(size_t limit) const {
    std::lock_guard lock(trades_mutex_);
    size_t count = std::min(limit, recent_trades_.size());
    return {recent_trades_.end() - static_cast<ptrdiff_t>(count), recent_trades_.end()};
}

SystemMetrics Exchange::metrics() const {
    return {orders_processed_.load(), 0.0, 0.0, last_trade_price_};
}

void Exchange::on_fill(const FillEvent& e) {
    last_trade_price_ = e.fill.fill_price;

    // Update user portfolios.
    {
        std::lock_guard lock(portfolios_mutex_);
        for (TraderId tid : {e.fill.maker_trader_id, e.fill.taker_trader_id}) {
            auto it = portfolios_.find(tid);
            if (it == portfolios_.end()) continue;
            bool is_maker = (tid == e.fill.maker_trader_id);
            it->second.apply_fill(is_maker ? Side::Sell : Side::Buy,
                                  e.fill.fill_price, e.fill.fill_qty);
        }
    }

    // Store in trade history.
    {
        std::lock_guard lock(trades_mutex_);
        recent_trades_.push_back(e.fill);
        if (recent_trades_.size() > MAX_TRADE_HISTORY)
            recent_trades_.pop_front();
    }

}
