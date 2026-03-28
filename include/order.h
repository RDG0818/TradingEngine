#pragma once
#include <cstdint>
#include <chrono>
#include <variant>

// Prices: fixed-point, 10000 units = $1.00  (e.g. $64,200 = 642,000,000)
// Quantities: integer units (e.g. 100 = 1.00 BTC at 2 decimal places)
using OrderId   = uint64_t;
using TraderId  = uint64_t;
using Price     = uint64_t;
using Quantity  = uint64_t;
using Timestamp = std::chrono::nanoseconds;

enum class Side         : uint8_t { Buy, Sell };
enum class TimeInForce  : uint8_t { GTC, IOC, FOK };

struct LimitOrder {
    OrderId    id;
    TraderId   trader_id;
    Side       side;
    Price      price;
    Quantity   qty;
    TimeInForce tif;
    Timestamp  ts;
};

struct MarketOrder {
    OrderId    id;
    TraderId   trader_id;
    Side       side;
    Quantity   qty;
    TimeInForce tif;
    Timestamp  ts;
};

struct StopLimitOrder {
    OrderId    id;
    TraderId   trader_id;
    Side       side;
    Price      stop_price;
    Price      limit_price;
    Quantity   qty;
    Timestamp  ts;
};

struct StopMarketOrder {
    OrderId    id;
    TraderId   trader_id;
    Side       side;
    Price      stop_price;
    Quantity   qty;
    Timestamp  ts;
};

using Order = std::variant<LimitOrder, MarketOrder, StopLimitOrder, StopMarketOrder>;

struct Fill {
    OrderId   maker_order_id;
    OrderId   taker_order_id;
    TraderId  maker_trader_id;
    TraderId  taker_trader_id;
    Price     fill_price;
    Quantity  fill_qty;
    Timestamp ts;
};

inline OrderId   get_order_id (const Order& o) { return std::visit([](const auto& x){ return x.id;        }, o); }
inline TraderId  get_trader_id(const Order& o) { return std::visit([](const auto& x){ return x.trader_id; }, o); }
inline Side      get_side     (const Order& o) { return std::visit([](const auto& x){ return x.side;      }, o); }
inline Quantity  get_qty      (const Order& o) { return std::visit([](const auto& x){ return x.qty;       }, o); }
inline Timestamp get_ts       (const Order& o) { return std::visit([](const auto& x){ return x.ts;        }, o); }
