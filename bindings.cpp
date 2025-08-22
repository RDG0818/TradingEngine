#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include "eventDispatcher.h"
#include "events.h"
#include "types.h"
#include "order.h"
#include "limitOrder.h"
#include "marketOrder.h"
#include "matchingEngine.h"

namespace py = pybind11;

PYBIND11_MODULE(trading_core, m) {
    m.doc() = "Python bindings for the C++ trading core";

    py::enum_<Side>(m, "Side")
        .value("BUY", Side::BUY)
        .value("SELL", Side::SELL)
        .export_values();

    py::enum_<OrderType>(m, "OrderType")
        .value("LIMIT", OrderType::LIMIT)
        .value("MARKET", OrderType::MARKET)
        .export_values();
    
    py::class_<Order, py::smart_holder>(m, "Order");

    py::class_<LimitOrder, Order, py::smart_holder>(m, "LimitOrder")
        .def(py::init<const std::string&, OrderID, OrderType, Side, std::string, Quantity, TraderID>())
        .def("get_price", &LimitOrder::getPrice)
        .def("get_quantity", &LimitOrder::getQuantity);
    // TODO: Add other necessary functions for limitOrder and marketOrder

    py::class_<MarketOrder, Order, py::smart_holder>(m, "MarketOrder")
        .def(py::init<const std::string&, OrderID, OrderType, Side, Quantity, TraderID>())
        .def("get_quantity", &MarketOrder::getQuantity);
    
    py::class_<MatchingEngine>(m, "MatchingEngine")
        .def(py::init<OrderBook&, EventDispatcher&>())
        .def("submit_order", &MatchingEngine::submitOrder, py::arg("order"))
        .def("cancel_order", &MatchingEngine::cancelOrder, py::arg("order_id"))
        .def("start", &MatchingEngine::start, py::call_guard<py::gil_scoped_release>())
        .def("stop", &MatchingEngine::stop);

    py::class_<TradeExecutedEvent>(m, "TradeExecutedEvent")
        .def(py::init<>())
        .def_readwrite("symbol", &TradeExecutedEvent::symbol)
        .def_readwrite("timestamp", &TradeExecutedEvent::timestamp)
        .def_readwrite("price", &TradeExecutedEvent::price)
        .def_readwrite("quantity", &TradeExecutedEvent::quantity)
        .def_readwrite("aggressing_order_id", &TradeExecutedEvent::aggressingOrderID)
        .def_readwrite("aggressing_trader_id", &TradeExecutedEvent::aggressingTraderID)
        .def_readwrite("aggressing_side", &TradeExecutedEvent::aggressingSide)
        .def_readwrite("aggressing_remaining_quantity", &TradeExecutedEvent::aggressingRemainingQuantity)
        .def_readwrite("resting_order_id", &TradeExecutedEvent::restingOrderID)
        .def_readwrite("resting_trader_id", &TradeExecutedEvent::restingTraderID)
        .def_readwrite("resting_remaining_quantity", &TradeExecutedEvent::restingRemainingQuantity)
        .def("__repr__",
            [](const TradeExecutedEvent &e) {
                return "<TradeExecutedEvent: aggressingOrderID=" + std::to_string(e.aggressingOrderID) + 
                ", restingOrderID=" + std::to_string(e.restingOrderID) +       
                ", price=" + std::to_string(e.price) +
                ", quantity=" + std::to_string(e.quantity) + ">";
            }
        );
    
    py::class_<OrderAcceptedEvent>(m, "OrderAcceptedEvent")
        .def(py::init<>())
        .def_readwrite("order_id", &OrderAcceptedEvent::orderID)
        .def_readwrite("price", &OrderAcceptedEvent::price)
        .def_readwrite("quantity", &OrderAcceptedEvent::quantity)
        .def("__repr__",
            [](const OrderAcceptedEvent &e) {
                return "<OrderAcceptedEvent: orderID=" + std::to_string(e.orderID) +
                ", price=" + std::to_string(e.price) +
                ", quantity=" + std::to_string(e.quantity) + ">";
            }
        );

    py::class_<OrderCancelledEvent>(m, "OrderCancelledEvent")
        .def(py::init<>())
        .def_readwrite("order_id", &OrderCancelledEvent::orderID)
        .def_readwrite("quantity", &OrderCancelledEvent::quantity)
        .def("__repr__",
            [](const OrderCancelledEvent &e) {
                return "<OrderCancelledEvent: orderID=" + std::to_string(e.orderID) +
                       ", quantity=" + std::to_string(e.quantity) + ">";
            }
        );
    py::class_<MarketDataEvent>(m, "MarketDataEvent")
        .def(py::init<>())
        .def_readwrite("symbol", &MarketDataEvent::symbol)
        .def_readwrite("last_price", &MarketDataEvent::last_price)
        .def_readwrite("timestamp", &MarketDataEvent::timestamp);

    py::class_<EventDispatcher>(m, "EventDispatcher")
        .def(py::init<>())
        .def("subscribe_trade_executed", &EventDispatcher::subscribe<TradeExecutedEvent>)
        .def("publish_trade_executed", &EventDispatcher::publish<TradeExecutedEvent>)
        .def("subscribe_order_accepted", &EventDispatcher::subscribe<OrderAcceptedEvent>)
        .def("publish_order_accepted", &EventDispatcher::publish<OrderAcceptedEvent>)
        .def("subscribe_order_cancelled", &EventDispatcher::subscribe<OrderCancelledEvent>)
        .def("publish_order_cancelled", &EventDispatcher::publish<OrderCancelledEvent>)      
        .def("subscribe_market_data", &EventDispatcher::subscribe<MarketDataEvent>)
        .def("publish_market_data", &EventDispatcher::publish<MarketDataEvent>);
        
    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<>());
    }