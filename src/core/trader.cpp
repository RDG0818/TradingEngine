#include "trader.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "symbolRegistry.h"

RandomMarketTrader::RandomMarketTrader(std::string name, MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, 
                                 float lambda, std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity)
    : Trader(name, TraderType::LIQUIDITY, engine, dispatcher, trader_id, symbols),
      lambda_(lambda),
      gen_(std::random_device{}()),
      exp_dist_{lambda},
      time_delta_s_(std::chrono::duration<double>(time_delta).count()),
      time_until_order_s_(0.0),
      side_dist_(0, 1),
      quantity_dist_(1, max_quantity),
      symbol_dist_(0, symbols.empty() ? 0 : symbols.size() - 1) {
    time_until_order_s_ = exp_dist_(gen_);
}

void RandomMarketTrader::tick() {
    if (symbols_.empty()) {
        return;
    }

    time_until_order_s_ -= time_delta_s_;

    while (time_until_order_s_ <= 0) {
        RawOrderParams raw_order_params = {
            .symbol = symbols_[symbol_dist_(gen_)],
            .order_type = OrderType::MARKET,
            .side = side_dist_(gen_) ? Side::BUY : Side::SELL,
            .price = "",
            .stop_price = "",
            .quantity = static_cast<Quantity>(quantity_dist_(gen_)),
            .trader_id = get_id(),
        };

        engine_.submit_order(raw_order_params);
        time_until_order_s_ += exp_dist_(gen_);
    }
}

void RandomMarketTrader::update_tick_interval(std::chrono::milliseconds new_interval) {
  time_delta_s_ = std::chrono::duration<double>(new_interval).count();
}

std::map<std::string, double> RandomMarketTrader::get_parameters() const {
  return {{"lambda", lambda_}};
}

void RandomMarketTrader::set_parameters(const std::map<std::string, double>& params) {
  if (params.count("lambda")) {
    lambda_ = params.at("lambda");
    exp_dist_.param(std::exponential_distribution<>::param_type(lambda_));
  }
}


RandomLimitTrader::RandomLimitTrader(std::string name, MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, float lambda, 
                           std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity, float norm_dist_var)
    : Trader(name, TraderType::RANDOM, engine, dispatcher, trader_id, symbols),
      lambda_(lambda),
      norm_dist_var_(norm_dist_var),
      gen_(std::random_device{}()),
      exp_dist_{lambda},
      time_delta_s_(std::chrono::duration<double>(time_delta).count()),
      time_until_order_s_(0.0),
      side_dist_(0, 1),
      quantity_dist_(1, max_quantity),
      symbol_dist_(0, symbols.empty() ? 0 : symbols.size() - 1),
      price_dist_(1, 100),
      norm_dist_(0, norm_dist_var_) {
    time_until_order_s_ = exp_dist_(gen_);
}

void RandomLimitTrader::tick() {
    if (symbols_.empty()) {
        return;
    }

    time_until_order_s_ -= time_delta_s_;

    while (time_until_order_s_ <= 0) {
        std::string symbol = symbols_[symbol_dist_(gen_)];
        SymbolID symbol_id = SymbolRegistry::get_instance().get_id(symbol);
        Side side = side_dist_(gen_) ? Side::BUY : Side::SELL;
        Price price = 0;

        std::optional<MarketData> best_bid = engine_.get_best_bid(symbol_id);
        std::optional<MarketData> best_ask = engine_.get_best_ask(symbol_id);

        if (!best_bid.has_value() && !best_ask.has_value()) {
            price = 1000000; // 100.00
        } else {
            if (side == Side::BUY) {
                if (!best_bid.has_value()) {
                    price = std::max(1UL, best_ask.value().price - price_dist_(gen_));
                } else {
                    price = std::max(1UL, static_cast<Price>(
                        std::round(best_bid.value().price + norm_dist_(gen_) * 10000)));
                }
            } else {  // Side::SELL
                if (!best_ask.has_value()) {
                    price = std::max(1UL, best_bid.value().price + price_dist_(gen_));
                } else {
                    price = std::max(1UL, static_cast<Price>(
                        std::round(best_ask.value().price + norm_dist_(gen_) * 10000)));
                }
            }
        }

        RawOrderParams raw_order_params = {
            .symbol = symbol,
            .order_type = OrderType::LIMIT,
            .side = side,
            .price = format_price(price), 
            .stop_price = "",
            .quantity = static_cast<Quantity>(quantity_dist_(gen_)),
            .trader_id = get_id(),
        };

        engine_.submit_order(raw_order_params);
        time_until_order_s_ += exp_dist_(gen_);
    }
}

void RandomLimitTrader::update_tick_interval(std::chrono::milliseconds new_interval) {
  time_delta_s_ = std::chrono::duration<double>(new_interval).count();
}

std::map<std::string, double> RandomLimitTrader::get_parameters() const {
  return {{"lambda", lambda_}, {"norm_dist_var", norm_dist_var_}};
}

void RandomLimitTrader::set_parameters(const std::map<std::string, double>& params) {
  if (params.count("lambda")) {
    lambda_ = params.at("lambda");
    exp_dist_.param(std::exponential_distribution<>::param_type(lambda_));
  }
  if (params.count("norm_dist_var")) {
    norm_dist_var_ = params.at("norm_dist_var");
    norm_dist_.param(std::normal_distribution<double>::param_type(0, norm_dist_var_));
  }
}

MarketMakerTrader::MarketMakerTrader(std::string name, MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id,
                                     const std::vector<std::string>& symbols, double mu, double sigma, double spread,
                                     std::chrono::milliseconds time_delta, int max_quantity,
                                     const std::unordered_map<std::string, double>& initial_prices)
    : Trader(name, TraderType::MARKET_MAKER, engine, dispatcher, trader_id, symbols),
      gen_(std::random_device{}()),
      norm_dist_(0.0, 1.0),
      quantity_dist_(1, max_quantity),
      mu_(mu),
      sigma_(sigma),
      spread_(spread),
      dt_(std::chrono::duration<double>(time_delta).count()),
      fair_prices_(initial_prices) {}

void MarketMakerTrader::tick() {
    if (symbols_.empty()) {
        return;
    }

    for (const auto& symbol : symbols_) {
        if (fair_prices_.find(symbol) == fair_prices_.end()) {
            continue;
        }

        double current_fair_price = fair_prices_.at(symbol);

        SymbolID sym_id = SymbolRegistry::get_instance().get_id(symbol);
        auto best_bid = engine_.get_best_bid(sym_id);
        auto best_ask = engine_.get_best_ask(sym_id);

        if (best_bid.has_value() && best_ask.has_value()) {
            double mid_price = (best_bid.value().price + best_ask.value().price) / 2.0 / 10000.0;
            current_fair_price = mid_price;
        }

        double w = norm_dist_(gen_);
        double new_fair_price = current_fair_price * std::exp((mu_ - 0.5 * sigma_ * sigma_) * dt_ + sigma_ * std::sqrt(dt_) * w);
        fair_prices_[symbol] = new_fair_price;

        double bid_price_double = new_fair_price * (1.0 - spread_);
        double ask_price_double = new_fair_price * (1.0 + spread_);
        
        Price bid_price_int = static_cast<Price>(std::round(bid_price_double * 10000));
        if (bid_price_int > 0) {
            RawOrderParams buy_rop = {
                .symbol = symbol,
                .order_type = OrderType::LIMIT,
                .side = Side::BUY,
                .price = format_price(bid_price_int), 
                .stop_price = "",
                .quantity = static_cast<Quantity>(quantity_dist_(gen_)),
                .trader_id = get_id(),
            };
            engine_.submit_order(buy_rop);
        }

        Price ask_price_int = static_cast<Price>(std::round(ask_price_double * 10000));
        if (ask_price_int > 0) {
            RawOrderParams sell_rop = {
                .symbol = symbol,
                .order_type = OrderType::LIMIT,
                .side = Side::SELL,
                .price = format_price(ask_price_int), 
                .stop_price = "",
                .quantity = static_cast<Quantity>(quantity_dist_(gen_)),
                .trader_id = get_id(),
            };
            engine_.submit_order(sell_rop);
        }
    }
}

void MarketMakerTrader::update_tick_interval(std::chrono::milliseconds new_interval) {
  dt_ = std::chrono::duration<double>(new_interval).count();
}

std::map<std::string, double> MarketMakerTrader::get_parameters() const {
  return {{"mu", mu_}, {"sigma", sigma_}, {"spread", spread_}};
}

void MarketMakerTrader::set_parameters(const std::map<std::string, double>& params) {
  if (params.count("mu")) {
    mu_ = params.at("mu");
  }
  if (params.count("sigma")) {
    sigma_ = params.at("sigma");
  }
  if (params.count("spread")) {
    spread_ = params.at("spread");
  }
}