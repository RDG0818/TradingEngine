#pragma once

#include "order.h"
#include "symbolRegistry.h"
#include "events.h"
#include "exceptions.h"
#include <string>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <functional>

struct RawOrderParams {
    std::string symbol;
    OrderType orderType;
    Side side;
    std::string price;
    std::string stopPrice;
    Quantity quantity;
    TraderID traderID;
    TimeInForce timeInForce = TimeInForce::GTC;
};

class OrderFactory {
public:
    static std::unique_ptr<Order> createOrder(const RawOrderParams& params, OrderID orderID) {
        SymbolID symbolID = SymbolRegistry::getInstance().getID(params.symbol);

        Price priceInt = 0;
        if (params.orderType == OrderType::LIMIT || params.orderType == OrderType::STOP_LIMIT) {
            if (!isValidPrice(params.price)) {
                throw InvalidPriceException("Invalid price format: " + params.price);
            }
            priceInt = convertPriceToInt(params.price);
        }

        Price stopPriceInt = 0;
        if (params.orderType == OrderType::STOP_MARKET || params.orderType == OrderType::STOP_LIMIT) {
            if (!isValidPrice(params.stopPrice)) {
                throw InvalidPriceException("Invalid stop price format: " + params.stopPrice);
            }
            stopPriceInt = convertPriceToInt(params.stopPrice);
        }

        if (params.quantity == 0) {
            throw InvalidQuantityException("Quantity must be positive.");
        }

        switch (params.orderType) {
            case OrderType::LIMIT:
                return std::make_unique<LimitOrder>(symbolID, orderID, params.side, priceInt, params.quantity, params.traderID, params.timeInForce);
            case OrderType::MARKET:
                return std::make_unique<MarketOrder>(symbolID, orderID, params.side, params.quantity, params.traderID);
            case OrderType::STOP_MARKET:
                return std::make_unique<StopMarketOrder>(symbolID, orderID, params.side, params.quantity, params.traderID, stopPriceInt);
            case OrderType::STOP_LIMIT:
                return std::make_unique<StopLimitOrder>(symbolID, orderID, params.side, params.quantity, params.traderID, stopPriceInt, priceInt, params.timeInForce);
            default:
                throw UnsupportedOrderTypeException("Unsupported order type.");
        }
    }

private:
    static bool isValidPrice(const std::string& priceStr) {
        if (priceStr.empty() || priceStr.find('-') != std::string::npos) return false;
        
        auto dot_pos = priceStr.find('.');
        if (dot_pos != std::string::npos && priceStr.length() - dot_pos - 1 > 4) {
            return false;
        }

        for (char c : priceStr) {
            if (!std::isdigit(c) && c != '.') {
                return false;
            }
        }
        return true;
    }

    static Price convertPriceToInt(const std::string& priceStr) {
        auto dot_pos = priceStr.find('.');
        
        if (dot_pos == std::string::npos) {
            return std::stoull(priceStr) * 10000;
        }

        long long whole_part = std::stoull(priceStr.substr(0, dot_pos));
        std::string frac_str = priceStr.substr(dot_pos + 1);

        if (frac_str.length() > 4) {
            frac_str = frac_str.substr(0, 4);
        }
        frac_str.resize(4, '0');
        
        long long frac_part = std::stoul(frac_str);

        return (whole_part * 10000) + frac_part;
    }
};
