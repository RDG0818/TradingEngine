// include/tradingSystem.h

#ifndef TRADINGENGINE_INCLUDE_TRADINGSYSTEM_H_
#define TRADINGENGINE_INCLUDE_TRADINGSYSTEM_H_

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <memory>

#include "matchingEngine.h"
#include "portfolio.h"
#include "traderManager.h"
#include "eventDispatcher.h"
#include "events.h"
#include "orderFactory.h"
#include "symbolRegistry.h"
#include "utils.h"


struct SystemMetrics {
  int64_t orders_processed = 0;
  double avg_latency_ms = 0.0; 
  int64_t active_orders = 0;
  size_t event_queue_depth = 0;
  double throughput = 0.0;
};

struct MarketSnapshot {
  Price best_bid = 0;
  Quantity best_bid_quantity = 0;
  Price best_ask = 0;
  Quantity best_ask_quantity = 0;
  Price last_trade_price = 0;
  Quantity last_trade_quantity = 0;
  std::deque<Price> recent_trades;
};

struct PortfolioSnapshot {
  Price balance = 0;
  std::map<SymbolID, Quantity> positions;
  std::deque<TradeExecutedEvent> trade_history;
};


class TradingSystem {
public:
  TradingSystem(int tick_interval_ms, const std::vector<std::string>& symbols);
  ~TradingSystem();

  void start();
  void stop();
  bool is_running() const;
  void enable_automated_traders(bool enable);
  bool are_automated_traders_enabled() const;

  bool add_random_market_trader(std::string name, float lambda, Quantity quantity); 

  bool add_random_limit_trader(std::string name, float lambda, Quantity quantity, float norm_dist_var);

  bool add_market_maker_trader(std::string name, float mu, float sigma, float spread, Quantity quantity, 
                            std::unordered_map<std::string, double>& init_price);

  bool remove_trader(const std::string& name);
  std::optional<std::map<std::string, double>> get_trader_parameters(const std::string& name);
  bool set_trader_parameters(const std::string& name, const std::map<std::string, double>& params);
  
  int get_tick_interval() const;
  bool set_tick_interval(int tick_length_ms);

  SystemMetrics get_system_metrics() const;
  std::optional<MarketSnapshot> get_market_snapshot(const std::string& symbol) const;
  std::optional<PortfolioSnapshot> get_portfolio_snapshot(TraderID trader_id) const;
  const std::vector<std::string>& get_all_symbols() const;

  TraderID create_portfolio(Price starting_balance);
  OrderID submit_order(TraderID trader_id, const RawOrderParams& params);
  void cancel_order(OrderID order_id);

private:
  void on_trade_executed(const TradeExecutedEvent& event);
  void on_book_update(const BookUpdateEvent& event);
  void on_order_accepted(const OrderAcceptedEvent& event);
  void on_order_filled(const OrderFilledEvent& event);
  void on_order_cancelled(const OrderCancelledEvent& event);

  EventDispatcher dispatcher_;
  OrderIdGenerator id_generator_;
  MatchingEngine engine_;
  std::vector<std::string> symbols_;
  TraderManager manager_;
  
  TraderID next_trader_id_ = 1;
  std::map<TraderID, Portfolio> portfolios_;
  std::map<OrderID, std::shared_ptr<Order>> live_orders_;

  mutable std::mutex system_mutex_; // A single mutex for all system state for simplicity
  SystemMetrics metrics_;
  std::map<SymbolID, MarketSnapshot> market_snapshots_;
  double avg_latency_ns_ = 0.0;
  
  mutable int64_t last_orders_processed_ = 0;
  mutable Timestamp last_metrics_query_time_;
  mutable double throughput_ = 0.0;
};

#endif // TRADINGENGINE_INCLUDE_TRADINGSYSTEM_H_
