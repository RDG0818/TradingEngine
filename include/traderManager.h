// include/traderManager.h

#ifndef TRADINGENGINE_INCLUDE_TRADERMANAGER_H_
#define TRADINGENGINE_INCLUDE_TRADERMANAGER_H_

#include <algorithm>
#include <mutex>
#include <optional>
#include <chrono>
#include "trader.h"

// class to run all Traders tick function on a single thread

struct TraderInfo {
  TraderID id;
  std::string name;
  TraderType type;
};

struct TraderMetrics {
  int64_t orders_submitted = 0;
  double orders_per_second = 0.0;
  double avg_latency_ms = 0.0;

  double avg_latency_ns_ = 0.0;
  int64_t last_orders_submitted_ = 0;
  Timestamp last_metrics_query_time_{std::chrono::system_clock::now()};
};

class TraderManager {

public:

  TraderManager(MatchingEngine& engine, 
                EventDispatcher& dispatcher,
                std::chrono::milliseconds tick_interval,
                std::vector<std::string>& symbols) 
              : engine_(engine), 
                dispatcher_(dispatcher), 
                tick_interval_(tick_interval),
                symbols_(symbols) {};

  ~TraderManager() {stop();};

  bool addRandomMarketTrader(std::string name, float lambda, Quantity quantity) {
      auto random_market_trader = 
        std::make_shared<RandomMarketTrader>(
          name,
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tick_interval_,
          symbols_,
          quantity 
      );
      return add_trader(std::move(random_market_trader));
    }

  bool addRandomLimitTrader(std::string name, float lambda, 
                            Quantity quantity, float norm_dist_var) {
      auto random_limit_trader = 
        std::make_shared<RandomLimitTrader>(
          name,
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tick_interval_,
          symbols_,
          quantity,
          norm_dist_var 
      );
      return add_trader(std::move(random_limit_trader)); 
  }

  bool addMarketMakerTrader(std::string name, float mu, float sigma, float spread,
                            Quantity quantity, std::unordered_map<std::string, double>& init_price) {
    auto market_maker_trader = 
      std::make_shared<MarketMakerTrader>(
        name,
        engine_,
        dispatcher_,
        trader_id_++, 
        symbols_,
        mu,
        sigma,
        spread,
        tick_interval_,
        quantity,
        init_price
    );
    return add_trader(std::move(market_maker_trader));
  }

  bool remove_trader(const std::string& name) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    auto it = std::find_if(traders_.begin(), traders_.end(), 
      [&](const auto& trader) { return trader->get_name() == name; });

    if (it != traders_.end()) {
      TraderID id_to_remove = (*it)->get_id();
      traders_.erase(it);

      std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
      trader_metrics_.erase(id_to_remove);
      return true;
    }
    return false;
  }

  std::optional<std::map<std::string, double>> get_trader_parameters(const std::string& name) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    Trader* trader = get_trader(name);
    if (trader) {
      return trader->get_parameters();
    }
    return std::nullopt;
  }

  bool set_trader_parameters(const std::string& name, 
      const std::map<std::string, double>& params) {
        std::lock_guard<std::mutex> lock(traders_mutex_);
        Trader* trader = get_trader(name);
        if (trader) {
          trader->set_parameters(params);
          return true;
        }
        return false;
  }

  bool start_trader(const std::string& name) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    Trader* trader = get_trader(name);
    if (trader) {
      trader->start();
      return true;
    }
    return false;
  }

  bool stop_trader(const std::string& name) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    Trader* trader = get_trader(name);
    if (trader) {
      trader->stop();
      return true;
    }
    return false;
  }

  std::optional<bool> is_trader_active(const std::string& name) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    Trader* trader = get_trader(name);
    if (trader) {
      return trader->is_active();
    }
    return std::nullopt;
  }

  int get_tick_interval() const {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    return tick_interval_.count();
  }

  std::vector<TraderInfo> get_all_traders() const {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    std::vector<TraderInfo> trader_infos;
    trader_infos.reserve(traders_.size());
    for (const auto& trader : traders_) {
      trader_infos.push_back({trader->get_id(), trader->get_name(), trader->get_type()});
    }
    return trader_infos;
  }

  bool set_tick_interval(int tick_length_ms) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    if (tick_length_ms <= 0) {
      // You might want to log an error here, e.g., LOG_ERROR("Tick interval must be positive.");
      return false; // Invalid tick length
    }

    auto new_interval = std::chrono::milliseconds(tick_length_ms);
    tick_interval_ = new_interval; 

    for (const auto& trader : traders_) {
      trader->update_tick_interval(new_interval);
    }

    return true;
  }

  void start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }
    thread_ = std::thread(&TraderManager::run, this);
  };

  void stop() {
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  };

  bool is_running() const {
    return running_;
  }

  void increment_order_count(TraderID trader_id) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (trader_metrics_.count(trader_id)) {
      trader_metrics_.at(trader_id).orders_submitted++;
    }
  }

  void update_latency(TraderID trader_id, int64_t latency_ns) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (trader_metrics_.count(trader_id)) {
      auto& metrics = trader_metrics_.at(trader_id);
      if (metrics.avg_latency_ns_ == 0.0) {
        metrics.avg_latency_ns_ = static_cast<double>(latency_ns);
      } else {
        metrics.avg_latency_ns_ = (static_cast<double>(latency_ns) * 0.05) + (metrics.avg_latency_ns_ * (1.0 - 0.05));
      }
    }
  }

  std::optional<TraderMetrics> get_metrics(TraderID trader_id) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (!trader_metrics_.count(trader_id)) {
      return std::nullopt;
    }

    auto& metrics = trader_metrics_.at(trader_id);
    auto now = std::chrono::system_clock::now();
    double time_delta_s = std::chrono::duration<double>(now - metrics.last_metrics_query_time_).count();

    if (time_delta_s > 0.9) {
      int64_t orders_delta = metrics.orders_submitted - metrics.last_orders_submitted_;
      metrics.orders_per_second = static_cast<double>(orders_delta) / time_delta_s;
      metrics.last_orders_submitted_ = metrics.orders_submitted;
      metrics.last_metrics_query_time_ = now;
    }

    metrics.avg_latency_ms = metrics.avg_latency_ns_ / 1e6;
    return metrics;
  }
  
private:

  MatchingEngine& engine_;
  EventDispatcher& dispatcher_;
  std::chrono::milliseconds tick_interval_;
  std::vector<std::string> symbols_;
  std::vector<std::shared_ptr<Trader>> traders_;
  std::atomic<bool> running_{false};
  std::atomic<TraderID> trader_id_ = 100000;
  std::thread thread_;
  mutable std::mutex traders_mutex_;

  mutable std::map<TraderID, TraderMetrics> trader_metrics_;
  mutable std::mutex metrics_mutex_;

  void run() {
    while (running_) {
      std::vector<std::shared_ptr<Trader>> traders_copy;
      std::chrono::milliseconds current_tick_interval; // Local copy for sleep_for
      {
        std::lock_guard<std::mutex> lock(traders_mutex_);
        traders_copy = traders_; // Make a copy under lock
        current_tick_interval = tick_interval_; // Get current interval under lock
      }

      for (auto& trader : traders_copy) {
        if (trader->is_active()) {
          trader->tick();
        }
      }
      std::this_thread::sleep_for(current_tick_interval); // Use the local copy
    }
  };

  bool add_trader(std::shared_ptr<Trader> trader) {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    auto it = std::find_if(traders_.begin(), traders_.end(),
      [&](const auto& existing_trader) { return existing_trader->get_name() == trader->get_name(); });

    if (it != traders_.end()) {
      return false; // Name already exists
    }

    TraderID new_id = trader->get_id();

    traders_.push_back(std::move(trader));

    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    trader_metrics_[new_id] = TraderMetrics{};

    return true;
  };

  Trader* get_trader(const std::string& name) {
    auto it = std::find_if(traders_.begin(), traders_.end(), 
      [&](const auto& trader) { return trader->get_name() == name; });
    
    if (it != traders_.end()) {
      return it->get();
    }
    return nullptr;
  }


};

#endif // TRADINGENGINE_INCLUDE_TRADERMANAGER_H_