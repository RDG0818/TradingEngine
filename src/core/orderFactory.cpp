// src/core/orderFactory.cpp

#include <stdexcept>
#include <string>

#include "events.h"
#include "order.h"
#include "orderFactory.h"
#include "symbolRegistry.h"
#include "utils.h"

namespace order_factory {

static bool is_valid_price(const std::string& price_str);
static Price convert_price_to_int(const std::string& price_str);

std::shared_ptr<Order> create_order(const RawOrderParams& params, OrderID order_id) {
    SymbolID symbol_id = SymbolRegistry::get_instance().get_id(params.symbol);

    Price price_int = 0;
    if (params.order_type == OrderType::LIMIT || params.order_type == OrderType::STOP_LIMIT) {
        if (!is_valid_price(params.price)) {
            throw InvalidPriceException("Invalid price format: " + params.price);
        }
        price_int = convert_price_to_int(params.price);
    }

    Price stop_price_int = 0;
    if (params.order_type == OrderType::STOP_MARKET || params.order_type == OrderType::STOP_LIMIT) {
        if (!is_valid_price(params.stop_price)) {
            throw InvalidPriceException("Invalid stop price format: " + params.stop_price);
        }
        stop_price_int = convert_price_to_int(params.stop_price);
    }

    if (params.quantity == 0) {
        throw InvalidQuantityException("Quantity must be positive.");
    }

    switch (params.order_type) {
        case OrderType::LIMIT:
            return std::make_shared<LimitOrder>(symbol_id, order_id, params.side, price_int, params.quantity, params.trader_id, params.time_in_force);
        case OrderType::MARKET:
            return std::make_shared<MarketOrder>(symbol_id, order_id, params.side, params.quantity, params.trader_id);
        case OrderType::STOP_MARKET:
            return std::make_shared<StopMarketOrder>(symbol_id, order_id, params.side, params.quantity, params.trader_id, stop_price_int);
        case OrderType::STOP_LIMIT:
            return std::make_shared<StopLimitOrder>(symbol_id, order_id, params.side, params.quantity, params.trader_id, stop_price_int, price_int, params.time_in_force);
        default:
            throw UnsupportedOrderTypeException("Unsupported order type.");
    }
}

static bool is_valid_price(const std::string& price_str) {
    if (price_str.empty() || price_str.find('-') != std::string::npos) return false;
    
    auto dot_pos = price_str.find('.');
    if (dot_pos != std::string::npos && price_str.length() - dot_pos - 1 > 4) {
        return false;
    }

    for (char c : price_str) {
        if (!std::isdigit(c) && c != '.') {
            return false;
        }
    }
    return true;
}

static Price convert_price_to_int(const std::string& price_str) {
    auto dot_pos = price_str.find('.');
    
    if (dot_pos == std::string::npos) {
        return std::stoull(price_str) * 10000;
    }

    long long whole_part = std::stoull(price_str.substr(0, dot_pos));
    std::string frac_str = price_str.substr(dot_pos + 1);

    if (frac_str.length() > 4) {
        frac_str = frac_str.substr(0, 4);
    }
    frac_str.resize(4, '0');
    
    long long frac_part = std::stoul(frac_str);

    return (whole_part * 10000) + frac_part;
}

} // namespace order_factory
