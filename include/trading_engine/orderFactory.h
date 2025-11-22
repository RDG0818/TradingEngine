#pragma once
#include "order.h"
#include "limitOrder.h"
#include "marketOrder.h"
#include "stopMarketOrder.h"
#include "stopLimitOrder.h"
#include "types.h"
#include <memory>
#include <string>
#include <stdexcept>
#include "symbolRegistry.h"


struct RawOrderParams {
    std::string symbol;
    TraderID traderID;
    Side side;
    OrderType orderType;
    Quantity quantity;
    Price price = 0;
    Price stopPrice = 0;
    TimeInForce tif = TimeInForce::GTC;
};

class UnsupportedOrderTypeException : public std::runtime_error {
public:
    UnsupportedOrderTypeException(const std::string& msg) : std::runtime_error(msg) {}
};

class InvalidPriceException : public std::runtime_error {
public:
    InvalidPriceException(const std::string& msg) : std::runtime_error(msg) {}
};

class InvalidQuantityException : public std::runtime_error {
public:
    InvalidQuantityException(const std::string& msg) : std::runtime_error(msg) {}
};

class OrderFactory {
public:
    static std::unique_ptr<Order> createOrder(const RawOrderParams& params, OrderID orderID);
};