// include/traderManager.h

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

  void addRandomMarketTrader(float lambda, std::chrono::milliseconds tickInterval, Quantity quantity) {
      auto random_market_trader = 
        std::make_unique<RandomMarketTrader>(
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tickInterval,
          symbols_,
          quantity 
      );
      addTrader(std::move(random_market_trader));
    }

  void addRandomLimitTrader(float lambda, std::chrono::milliseconds tickInterval, 
                            Quantity quantity, float norm_dist_var) {
      auto random_limit_trader = 
        std::make_unique<RandomLimitTrader>(
          engine_,
          dispatcher_,
          trader_id_++, 
          lambda,
          tickInterval,
          symbols_,
          quantity,
          norm_dist_var 
      );
      addTrader(std::move(random_limit_trader)); 
  }

  void addMarketMakerTrader(float mu, float sigma, float spread,
                            std::chrono::milliseconds tickInterval, 
                            Quantity quantity, 
                            std::unordered_map<std::string, double>& init_price) {
    auto market_maker_trader = 
      std::make_unique<MarketMakerTrader>(
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
    addTrader(std::move(market_maker_trader));
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
  std::vector<std::unique_ptr<Trader>> traders_;
  std::atomic<bool> running_{false};
  std::atomic<TraderID> trader_id_ = 100000;
  std::thread thread_;

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

  void addTrader(std::unique_ptr<Trader> trader) {
    traders_.push_back(std::move(trader));
  };

};