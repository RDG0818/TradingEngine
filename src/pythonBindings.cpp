#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/operators.h>

#include "tradingSystem.h"
#include "orderFactory.h"
#include "utils.h"
#include "events.h"

namespace py = pybind11;

PYBIND11_MODULE(trading_engine_py, m) {
  m.doc() = "Python bindings for the trading engine";

  // Utils
  py::enum_<OrderType>(m, "OrderType")
    .value("LIMIT", OrderType::LIMIT)
    .value("MARKET", OrderType::MARKET)
    .value("STOP_MARKET", OrderType::STOP_MARKET)
    .value("STOP_LIMIT", OrderType::STOP_LIMIT)
    .export_values();

  py::enum_<Side>(m, "Side")
    .value("BUY", Side::BUY)
    .value("SELL", Side::SELL)
    .export_values();

  py::enum_<TimeInForce>(m, "TimeInForce")
    .value("GTC", TimeInForce::GTC)
    .value("IOC", TimeInForce::IOC)
    .value("FOK", TimeInForce::FOK)
    .export_values();

  py::enum_<TraderType>(m, "TraderType")
    .value("RANDOM_MARKET", TraderType::RANDOM_MARKET)
    .value("RANDOM_LIMIT", TraderType::RANDOM_LIMIT)
    .value("MARKET_MAKER", TraderType::MARKET_MAKER)
    .value("MOMENTUM", TraderType::MOMENTUM)
    .value("MEAN_REVERSION", TraderType::MEAN_REVERSION)
    .export_values();

  py::class_<RawOrderParams>(m, "RawOrderParams")
    .def(py::init<>())
    .def_readwrite("symbol", &RawOrderParams::symbol)
    .def_readwrite("order_type", &RawOrderParams::order_type)
    .def_readwrite("side", &RawOrderParams::side)
    .def_readwrite("price", &RawOrderParams::price)
    .def_readwrite("stop_price", &RawOrderParams::stop_price)
    .def_readwrite("quantity", &RawOrderParams::quantity)
    .def_readwrite("trader_id", &RawOrderParams::trader_id)
    .def_readwrite("time_in_force", &RawOrderParams::time_in_force);

  // Metrics and Snapshots
  py::class_<SystemMetrics>(m, "SystemMetrics")
    .def(py::init<>())
    .def_readonly("orders_processed", &SystemMetrics::orders_processed)
    .def_readonly("avg_latency_ms", &SystemMetrics::avg_latency_ms)
    .def_readonly("active_orders", &SystemMetrics::active_orders)
    .def_readonly("event_queue_depth", &SystemMetrics::event_queue_depth)
    .def_readonly("throughput", &SystemMetrics::throughput);

  py::class_<TraderMetrics>(m, "TraderMetrics")
    .def(py::init<>())
    .def_readonly("orders_submitted", &TraderMetrics::orders_submitted)
    .def_readonly("orders_per_second", &TraderMetrics::orders_per_second)
    .def_readonly("avg_latency_ms", &TraderMetrics::avg_latency_ms);

  py::class_<TraderInfo>(m, "TraderInfo")
  .def(py::init<>())
  .def_readonly("id", &TraderInfo::id)
  .def_readonly("name", &TraderInfo::name)
  .def_readonly("type", &TraderInfo::type);

  py::class_<MarketSnapshot>(m, "MarketSnapshot")
    .def(py::init<>())
    .def_readonly("best_bid", &MarketSnapshot::best_bid)
    .def_readonly("best_bid_quantity", &MarketSnapshot::best_bid_quantity)
    .def_readonly("best_ask", &MarketSnapshot::best_ask)
    .def_readonly("best_ask_quantity", &MarketSnapshot::best_ask_quantity)
    .def_readonly("last_trade_price", &MarketSnapshot::last_trade_price)
    .def_readonly("last_trade_quantity", &MarketSnapshot::last_trade_quantity)
    .def_readonly("recent_trades", &MarketSnapshot::recent_trades);
      
  py::class_<TradeExecutedEvent>(m, "TradeExecutedEvent")
    .def(py::init<SymbolID, Price, Quantity, OrderID, TraderID, Side, Quantity, OrderID, TraderID, Quantity>())
    .def_readonly("symbol_id", &TradeExecutedEvent::symbol_id)
    .def_readonly("price", &TradeExecutedEvent::price)
    .def_readonly("quantity", &TradeExecutedEvent::quantity)
    .def_readonly("aggressing_order_id", &TradeExecutedEvent::aggressing_order_id)
    .def_readonly("aggressing_trader_id", &TradeExecutedEvent::aggressing_trader_id)
    .def_readonly("aggressing_side", &TradeExecutedEvent::aggressing_side)
    .def_readonly("aggressing_order_remaining_quantity", &TradeExecutedEvent::aggressing_order_remaining_quantity)
    .def_readonly("resting_order_id", &TradeExecutedEvent::resting_order_id)
    .def_readonly("resting_trader_id", &TradeExecutedEvent::resting_trader_id)
    .def_readonly("resting_order_remaining_quantity", &TradeExecutedEvent::resting_order_remaining_quantity);

  py::class_<PortfolioSnapshot>(m, "PortfolioSnapshot")
    .def(py::init<>())
    .def_readonly("balance", &PortfolioSnapshot::balance)
    .def_readonly("positions", &PortfolioSnapshot::positions)
    .def_readonly("trade_history", &PortfolioSnapshot::trade_history);

  // Main TradingSystem
  py::class_<TradingSystem>(m, "TradingSystem")
    .def(py::init<int, const std::vector<std::string>&>(), py::arg("tick_interval_ms"), py::arg("symbols"))
    .def("start", &TradingSystem::start)
    .def("stop", &TradingSystem::stop)
    .def("is_running", &TradingSystem::is_running)
    .def("add_random_market_trader", &TradingSystem::add_random_market_trader)
    .def("add_random_limit_trader", &TradingSystem::add_random_limit_trader)
    .def("add_market_maker_trader", &TradingSystem::add_market_maker_trader)
    .def("remove_trader", &TradingSystem::remove_trader)
    .def("start_trader", &TradingSystem::start_trader)
    .def("stop_trader", &TradingSystem::stop_trader)
    .def("is_trader_active", &TradingSystem::is_trader_active)
    .def("get_trader_parameters", &TradingSystem::get_trader_parameters)
    .def("set_trader_parameters", &TradingSystem::set_trader_parameters)
    .def("enable_automated_traders", &TradingSystem::enable_automated_traders)
    .def("are_automated_traders_enabled", &TradingSystem::are_automated_traders_enabled)
    .def("get_system_metrics", &TradingSystem::get_system_metrics)
    .def("get_trader_metrics", &TradingSystem::get_trader_metrics, py::arg("trader_id"))
    .def("get_market_snapshot", &TradingSystem::get_market_snapshot, py::arg("symbol"))
    .def("get_portfolio_snapshot", &TradingSystem::get_portfolio_snapshot, py::arg("trader_id"))
    .def("get_all_traders", &TradingSystem::get_all_traders)
    .def("get_tick_interval", &TradingSystem::get_tick_interval)
    .def("set_tick_interval", &TradingSystem::set_tick_interval, py::arg("tick_length_ms"))
    .def("get_all_symbols", &TradingSystem::get_all_symbols)
    .def("create_portfolio", &TradingSystem::create_portfolio, py::arg("starting_balance"))
    .def("reset_balance", &TradingSystem::reset_balance, py::arg("trader_id"))
    .def("submit_order", &TradingSystem::submit_order, py::arg("trader_id"), py::arg("params"))
    .def("cancel_order", &TradingSystem::cancel_order, py::arg("order_id"));
}