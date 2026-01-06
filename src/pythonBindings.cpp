#include <pybind11/pybind11.h>
#include "order.h"
#include "utils.h"

namespace py = pybind11;

PYBIND11_MODULE(trading_engine_py, m) {
  py::enum_<OrderType>(m, "OrderType")
    .value("MARKET", OrderType::MARKET)
    .value("LIMIT", OrderType::LIMIT)
    .value("STOP_MARKET", OrderType::STOP_MARKET)
    .value("STOP_LIMIT", OrderType::STOP_LIMIT)
    .export_values();

  py::enum_<Side>(m, "Side")
    .value("BUY", Side::BUY)
    .value("SELL", Side::SELL)
    .export_values();

  py::enum_<OrderStatus>(m, "OrderStatus")
    .value("NEW", OrderStatus::NEW)
    .value("ACCEPTED", OrderStatus::ACCEPTED)
    .value("REJECTED", OrderStatus::REJECTED)
    .value("PARTIALLY_FILLED", OrderStatus::PARTIALLY_FILLED)
    .value("FILLED", OrderStatus::FILLED)
    .value("CANCELLED", OrderStatus::CANCELLED)
    .export_values();

  py::enum_<TimeInForce>(m, "TimeInForce")
    .value("GTC", TimeInForce::GTC)
    .value("IOC", TimeInForce::IOC)
    .value("FOK", TimeInForce::FOK)
    .export_values();

  py::class_<Order>(m, "Order")
    .def_property_readonly("get_order_id", &Order::get_order_id)
    .def_property_readonly("get_symbol_id", &Order::get_symbol_id)
    .def_property_readonly("get_order_type", &Order::get_order_type)
    .def_property_readonly("get_order_status", &Order::get_order_status)
    .def_property_readonly("get_trader_id", &Order::get_trader_id)
    .def_property_readonly("get_time_in_force", &Order::get_time_in_force)
    .def_property_readonly("get_side", &Order::get_side)
    .def_property("get_quantity", &Order::get_quantity, &Order::set_quantity)
    .def("get_price", &Order::get_price)
    .def("get_stop_price", &Order::get_stop_price);

  py::class_<LimitOrder, Order>(m, "LimitOrder")
    .def(py::init<SymbolID, OrderID, Side, Price, Quantity, TraderID, TimeInForce>(),
        py::arg("symbol_id"), py::arg("order_id"), py::arg("side"),
        py::arg("price"), py::arg("quantity"), py::arg("trader_id"),
        py::arg("tif") = TimeInForce::GTC)
    .def_property_readonly("get_price", &LimitOrder::get_price);

  py::class_<MarketOrder, Order>(m, "MarketOrder")
    .def(py::init<SymbolID, OrderID, Side, Quantity, TraderID>(),
        py::arg("symbol_id"), py::arg("order_id"), py::arg("side"),
        py::arg("quantity"), py::arg("trader_id")
    );
}