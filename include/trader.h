//include/trader.h

#ifndef TRADINGENGINE_INCLUDE_TRADER_H_
#define TRADINGENGINE_INCLUDE_TRADER_H_

#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "eventDispatcher.h"
#include "events.h"
#include "matchingEngine.h"
#include "symbolRegistry.h"

// TODO: subcribe each trader to events for submission verification

enum class TraderType : std::uint8_t {
    RANDOM,
    MARKET_MAKER,
    MOMENTUM,
    MEAN_REVERSION,
    LIQUIDITY
};

class Trader {

public:

  Trader(TraderType type, MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, const std::vector<std::string>& symbols) 
    : engine_(engine), dispatcher_(dispatcher), symbols_(symbols), type_(type), trader_id_(trader_id) {}
  virtual ~Trader() = default;

  virtual void tick() = 0;
    
  TraderType get_type() const { return type_; }
  TraderID get_id() const { return trader_id_; }

protected:

  MatchingEngine& engine_;
  EventDispatcher& dispatcher_;
  std::vector<std::string> symbols_;

private:

  TraderType type_;
  TraderID trader_id_;

};


// Lambda is average per second
// TODO: Add more complex logic 
class RandomMarketTrader : public Trader {

public:

  RandomMarketTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, 
    float lambda, std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity);

  void tick() override;

private:

  std::mt19937 gen_;
  std::exponential_distribution<> exp_dist_;
  double time_delta_s_;
  double time_until_order_s_;

  std::uniform_int_distribution<int> side_dist_;
  std::uniform_int_distribution<int> quantity_dist_;
  std::uniform_int_distribution<int> symbol_dist_;

};


// TODO: Add more complex logic here
class RandomLimitTrader : public Trader {

public:

  RandomLimitTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, float lambda, 
    std::chrono::milliseconds time_delta, const std::vector<std::string>& symbols, int max_quantity, float norm_dist_var);
    
  void tick() override;

private:

  std::mt19937 gen_;
  std::exponential_distribution<> exp_dist_;
  double time_delta_s_;
  double time_until_order_s_;

  std::uniform_int_distribution<int> side_dist_;
  std::uniform_int_distribution<int> quantity_dist_;
  std::uniform_int_distribution<int> symbol_dist_;
  std::uniform_int_distribution<int> price_dist_;
  std::normal_distribution<double> norm_dist_;

};

// TODO: Add inventory skew to this
// TODO: comment code
// TODO: Add more complex logic
class MarketMakerTrader : public Trader {

public:

  MarketMakerTrader(MatchingEngine& engine, EventDispatcher& dispatcher, TraderID trader_id, 
    const std::vector<std::string>& symbols, double mu, double sigma, double spread,
    std::chrono::milliseconds time_delta, int max_quantity, 
    const std::unordered_map<std::string, double>& initial_prices);

  void tick() override;

private:

  std::mt19937 gen_;
  std::normal_distribution<double> norm_dist_;
  std::uniform_int_distribution<int> quantity_dist_;

  double mu_;
  double sigma_;
  double spread_;
  double dt_;

  std::unordered_map<std::string, double> fair_prices_;

};


// TODO: Add other two traders from enum
#endif // TRADINGENGINE_INCLUDE_TRADER_H_