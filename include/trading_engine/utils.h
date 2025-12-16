// include/trading_engine/utils.h
#pragma once
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <string>

// Type Aliasing

using Price = std::uint64_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;
using TraderID = std::uint32_t;
using SymbolID = std::uint32_t;
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

// Enumeration

enum class Side : std::uint8_t { BUY, SELL };

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

// Custom Exceptions

class InvalidPriceException : public std::invalid_argument {
public:
    explicit InvalidPriceException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

class InvalidQuantityException : public std::invalid_argument {
public:
    explicit InvalidQuantityException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

class UnsupportedOrderTypeException : public std::invalid_argument {
public:
    explicit UnsupportedOrderTypeException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};
