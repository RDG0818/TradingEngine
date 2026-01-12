// include/portfolio.h

#include <deque>
#include <memory>
#include <map>

#include "events.h"
#include "order.h"
#include "matchingEngine.h"
#include "utils.h"

class Portfolio {

public:

  Portfolio(MatchingEngine& engine, Price balance, TraderID trader_id) 
          : engine_(engine), 
            balance_(balance), 
            trader_id_(trader_id) {starting_balance_ = balance_; };
  Price get_balance() const;
  TraderID get_trader_id() const;
  Quantity get_position(SymbolID symbol_id) const;
  const std::deque<std::shared_ptr<Order>>& get_order_history() const;
  const std::deque<TradeExecutedEvent>& get_trade_history() const;
  bool can_submit_order(const std::shared_ptr<Order>& order) const;
  void on_trade_executed(const TradeExecutedEvent& trade, const std::shared_ptr<Order>& order);
  const std::map<SymbolID, Quantity>& get_all_positions() const;
  void reset_balance();

private:

  MatchingEngine& engine_;
  Price balance_;
  TraderID trader_id_;
  std::map<SymbolID, Quantity> assets_;
  Price starting_balance_;

  std::deque<std::shared_ptr<Order>> order_history_;
  static constexpr size_t MAX_ORDER_HISTORY_SIZE = 1000;

  std::deque<TradeExecutedEvent> trade_history_;
  static constexpr size_t MAX_TRADE_HISTORY_SIZE = 1000;
};