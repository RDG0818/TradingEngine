#pragma once
#include <cstdint>
#include <chrono>

using Price = std::uint64_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;
using TraderID = std::uint32_t;
using SymbolID = std::uint32_t;
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

enum class Side : std::uint8_t { BUY, SELL };

constexpr Side getOppositeSide(Side side) {
    return (side == Side::BUY) ? Side::SELL : Side::BUY;
};

enum class OrderType : std::uint8_t {
    LIMIT,
    MARKET,
    STOP_MARKET,
    STOP_LIMIT
};

enum class OrderStatus : std::uint8_t {
    NEW,
    ACCEPTED,
    REJECTED,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED
};

enum class TimeInForce : std::uint8_t {
    GTC,
    IOC,
    FOK
};