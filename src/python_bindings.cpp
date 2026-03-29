// src/python_bindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "exchange.h"
#include "market_events.h"

namespace py = pybind11;

PYBIND11_MODULE(trading_engine_py, m) {
    m.doc() = "Talat trading engine — C++ core";

    py::enum_<Side>(m, "Side")
        .value("Buy",  Side::Buy)
        .value("Sell", Side::Sell);

    py::enum_<TimeInForce>(m, "TimeInForce")
        .value("GTC", TimeInForce::GTC)
        .value("IOC", TimeInForce::IOC)
        .value("FOK", TimeInForce::FOK);

    py::enum_<MarketEventType>(m, "MarketEventType")
        .value("FlashCrash",         MarketEventType::FlashCrash)
        .value("BullRun",            MarketEventType::BullRun)
        .value("LiquiditySqueeze",   MarketEventType::LiquiditySqueeze)
        .value("MeanReversionTrap",  MarketEventType::MeanReversionTrap);

    py::class_<BookSnapshot>(m, "BookSnapshot")
        .def_readonly("bids", &BookSnapshot::bids)
        .def_readonly("asks", &BookSnapshot::asks);

    py::class_<Fill>(m, "Fill")
        .def_readonly("maker_order_id",  &Fill::maker_order_id)
        .def_readonly("taker_order_id",  &Fill::taker_order_id)
        .def_readonly("maker_trader_id", &Fill::maker_trader_id)
        .def_readonly("taker_trader_id", &Fill::taker_trader_id)
        .def_readonly("fill_price",      &Fill::fill_price)
        .def_readonly("fill_qty",        &Fill::fill_qty);

    py::class_<SystemMetrics>(m, "SystemMetrics")
        .def_readonly("orders_processed",  &SystemMetrics::orders_processed)
        .def_readonly("avg_latency_us",    &SystemMetrics::avg_latency_us)
        .def_readonly("throughput_per_s",  &SystemMetrics::throughput_per_s)
        .def_readonly("last_trade_price",  &SystemMetrics::last_trade_price);

    py::class_<PortfolioSnapshot>(m, "PortfolioSnapshot")
        .def_readonly("balance",         &PortfolioSnapshot::balance)
        .def_readonly("position",        &PortfolioSnapshot::position)
        .def_readonly("unrealized_pnl",  &PortfolioSnapshot::unrealized_pnl)
        .def_readonly("avg_cost",        &PortfolioSnapshot::avg_cost);

    py::class_<TraderMetrics>(m, "TraderMetrics")
        .def_readonly("orders_per_second", &TraderMetrics::orders_per_second)
        .def_readonly("pnl",               &TraderMetrics::pnl)
        .def_readonly("position",          &TraderMetrics::position);

    py::class_<TraderInfo>(m, "TraderInfo")
        .def_readonly("id",      &TraderInfo::id)
        .def_readonly("name",    &TraderInfo::name)
        .def_readonly("type",    &TraderInfo::type)
        .def_readonly("active",  &TraderInfo::active)
        .def_readonly("metrics", &TraderInfo::metrics);

    py::class_<MarketEventInfo>(m, "MarketEventInfo")
        .def_readonly("id",                 &MarketEventInfo::id)
        .def_readonly("name",               &MarketEventInfo::name)
        .def_readonly("description",        &MarketEventInfo::description)
        .def_readonly("default_duration_s", &MarketEventInfo::default_duration_s);

    m.def("all_market_events", &all_market_events);

    py::class_<Exchange>(m, "Exchange")
        .def(py::init<>())
        .def("start",              &Exchange::start,              py::arg("seed_price"))
        .def("stop",               &Exchange::stop)
        .def("is_running",         &Exchange::is_running)
        .def("create_portfolio",   &Exchange::create_portfolio,   py::arg("balance"))
        .def("portfolio_snapshot", &Exchange::portfolio_snapshot, py::arg("trader_id"))
        .def("book_snapshot",      &Exchange::book_snapshot)
        .def("recent_trades",      &Exchange::recent_trades,      py::arg("limit") = 50)
        .def("metrics",            &Exchange::metrics)
        .def("all_traders",        &Exchange::all_traders)
        .def("remove_trader",      &Exchange::remove_trader,      py::arg("trader_id"))
        .def("start_trader",       &Exchange::start_trader,       py::arg("trader_id"))
        .def("stop_trader",        &Exchange::stop_trader,        py::arg("trader_id"))
        .def("trigger_event",      &Exchange::trigger_event,
             py::arg("type"), py::arg("duration_ticks") = 30)
        .def("cancel_order",       &Exchange::cancel_order,       py::arg("order_id"))
        // Python callback hooks (with GIL acquire).
        .def("on_fill_callback", [](Exchange& ex, py::object cb) {
            ex.on_fill_callback([cb](const Fill& f) {
                py::gil_scoped_acquire gil;
                cb(f);
            });
        }, py::arg("callback"))
        .def("on_book_update_callback", [](Exchange& ex, py::object cb) {
            ex.on_book_update_callback([cb](const BookSnapshot& snap) {
                py::gil_scoped_acquire gil;
                cb(snap);
            });
        }, py::arg("callback"))
        // Order submission.
        .def("submit_limit_order", [](Exchange& ex, uint64_t trader_id, bool is_buy,
                                       uint64_t price, uint64_t qty, const std::string& tif_str) {
            static std::atomic<OrderId> next_id{500000};
            TimeInForce tif = TimeInForce::GTC;
            if (tif_str == "IOC") tif = TimeInForce::IOC;
            if (tif_str == "FOK") tif = TimeInForce::FOK;
            LimitOrder o;
            o.id        = next_id.fetch_add(1);
            o.trader_id = trader_id;
            o.side      = is_buy ? Side::Buy : Side::Sell;
            o.price     = price;
            o.qty       = qty;
            o.tif       = tif;
            o.ts        = std::chrono::steady_clock::now().time_since_epoch();
            ex.submit_order(Order{o});
            return o.id;
        }, py::arg("trader_id"), py::arg("is_buy"), py::arg("price"),
           py::arg("qty"), py::arg("tif") = "GTC")
        .def("submit_market_order", [](Exchange& ex, uint64_t trader_id, bool is_buy, uint64_t qty) {
            static std::atomic<OrderId> next_id{600000};
            MarketOrder o;
            o.id        = next_id.fetch_add(1);
            o.trader_id = trader_id;
            o.side      = is_buy ? Side::Buy : Side::Sell;
            o.qty       = qty;
            o.tif       = TimeInForce::IOC;
            o.ts        = std::chrono::steady_clock::now().time_since_epoch();
            ex.submit_order(Order{o});
            return o.id;
        }, py::arg("trader_id"), py::arg("is_buy"), py::arg("qty"))
        // Trader factory methods.
        .def("add_market_maker", [](Exchange& ex, const std::string& name,
                                    uint64_t balance, uint64_t seed_price) {
            return ex.add_trader<MarketMakerTrader>(name, balance, seed_price);
        }, py::arg("name"), py::arg("balance"), py::arg("seed_price"))
        .def("add_momentum_trader", [](Exchange& ex, const std::string& name, uint64_t balance) {
            return ex.add_trader<MomentumTrader>(name, balance);
        }, py::arg("name"), py::arg("balance"))
        .def("add_mean_reversion_trader", [](Exchange& ex, const std::string& name, uint64_t balance) {
            return ex.add_trader<MeanReversionTrader>(name, balance);
        }, py::arg("name"), py::arg("balance"))
        .def("add_twap_trader", [](Exchange& ex, const std::string& name, uint64_t balance,
                                    uint64_t total_qty, int slices, bool is_buy) {
            return ex.add_trader<TWAPTrader>(name, balance, total_qty, slices,
                                             is_buy ? Side::Buy : Side::Sell);
        }, py::arg("name"), py::arg("balance"), py::arg("total_qty"),
           py::arg("slices"), py::arg("is_buy"))
        .def("add_trend_follower", [](Exchange& ex, const std::string& name, uint64_t balance) {
            return ex.add_trader<TrendFollowerTrader>(name, balance);
        }, py::arg("name"), py::arg("balance"))
        .def("add_random_limit_trader", [](Exchange& ex, const std::string& name, uint64_t balance) {
            return ex.add_trader<RandomLimitTrader>(name, balance);
        }, py::arg("name"), py::arg("balance"))
        .def("add_random_market_trader", [](Exchange& ex, const std::string& name, uint64_t balance) {
            return ex.add_trader<RandomMarketTrader>(name, balance);
        }, py::arg("name"), py::arg("balance"));
}
