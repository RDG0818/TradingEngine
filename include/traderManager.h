// include/traderManager.h

#include <algorithm>
#include <mutex>
#include <optional>
#include "trader.h"

// class to run all Traders tick function on a single thread

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
    auto it = std::remove_if(traders_.begin(), traders_.end(), 
      [&](const auto& trader) { return trader->get_name() == name; });

    if (it != traders_.end()) {
      traders_.erase(it, traders_.end());
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

  int get_tick_interval() const {
    std::lock_guard<std::mutex> lock(traders_mutex_);
    return tick_interval_.count();
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

  std::random_device rd;
  std::mt19937 gen;

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
        trader->tick();
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

    traders_.push_back(std::move(trader));
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