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

  void addRandomMarketTrader(std::string name, float lambda, std::chrono::milliseconds tickInterval, Quantity quantity) {
      auto random_market_trader = 
        std::make_shared<RandomMarketTrader>(
          name,
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tickInterval,
          symbols_,
          quantity 
      );
      add_trader(std::move(random_market_trader));
    }

  void addRandomLimitTrader(std::string name, float lambda, std::chrono::milliseconds tickInterval, 
                            Quantity quantity, float norm_dist_var) {
      auto random_limit_trader = 
        std::make_shared<RandomLimitTrader>(
          name,
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tickInterval,
          symbols_,
          quantity,
          norm_dist_var 
      );
      add_trader(std::move(random_limit_trader)); 
  }

  void addMarketMakerTrader(std::string name, float mu, float sigma, float spread,
                            std::chrono::milliseconds tickInterval, 
                            Quantity quantity, 
                            std::unordered_map<std::string, double>& init_price) {
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
        tickInterval,
        quantity,
        init_price
    );
    add_trader(std::move(market_maker_trader));
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
      for (auto& trader : traders_) {
        trader->tick();
      }
      std::this_thread::sleep_for(tick_interval_);
    }
  };

  void add_trader(std::shared_ptr<Trader> trader) {
    traders_.push_back(std::move(trader));
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