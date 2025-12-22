// include/utils.h

#ifndef TRADINGENGINE_INCLUDE_UTILS_H_
#define TRADINGENGINE_INCLUDE_UTILS_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <iostream> 

// Debugging Utilities

#ifndef NDEBUG
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " " << msg << std::endl
#else
#define LOG_DEBUG(msg) (void)0
#endif

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

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

// Utility Functions

inline std::string format_price(Price price_int) {
    if (price_int == 0) return "0.0000";
    std::stringstream ss;
    ss << price_int / 10000 << '.' 
        << std::setw(4) << std::setfill('0') << price_int % 10000;
    return ss.str();
}

inline std::string ToString(Side side) {
    switch (side) {
        case Side::BUY: return "BUY";
        case Side::SELL: return "SELL";
    }
    return "UNKNOWN_SIDE";
}

inline std::string ToString(OrderType type) {
    switch (type) {
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        case OrderType::STOP_MARKET: return "STOP_MARKET";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
    }
    return "UNKNOWN_ORDER_TYPE";
}

inline std::string ToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::NEW: return "NEW";
        case OrderStatus::ACCEPTED: return "ACCEPTED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN_ORDER_STATUS";
}

inline std::string ToString(TimeInForce tif) {
    switch (tif) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
    }
    return "UNKNOWN_TIME_IN_FORCE";
}

inline std::string FormatTimestamp(Timestamp ts) {
    auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch());
    auto seconds_part = std::chrono::duration_cast<std::chrono::seconds>(ms_since_epoch);
    auto ms_part = ms_since_epoch - seconds_part;

    std::time_t t = seconds_part.count();
    std::tm tm = *std::localtime(&t); // TODO: Use localtime_s (Windows) or localtime_r (POSIX) for thread-safety

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms_part.count();
    return oss.str();
}


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

#endif  // TRADINGENGINE_INCLUDE_UTILS_H_
