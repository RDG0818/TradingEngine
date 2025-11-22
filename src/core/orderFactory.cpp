#include "trading_engine/orderFactory.h"

std::unique_ptr<Order> OrderFactory::createOrder(const RawOrderParams& params, OrderID orderID) {
    SymbolID symbolID = SymbolRegistry::getInstance().getID(params.symbol);

    if (params.quantity == 0) {
        throw InvalidQuantityException("Order quantity cannot be zero.");
    }
    
    switch (params.orderType) {
        case OrderType::LIMIT:
            if (params.price == 0) {
                throw InvalidPriceException("Limit orders must have a price.");
            }
            return std::make_unique<LimitOrder>(symbolID, orderID, params.side, params.price, params.quantity, params.traderID, params.tif);
        case OrderType::MARKET:
            return std::make_unique<MarketOrder>(symbolID, orderID, params.side, params.quantity, params.traderID);
        case OrderType::STOP_MARKET:
            if (params.stopPrice == 0) {
                throw InvalidPriceException("Stop Market orders must have a stop price.");
            }
            return std::make_unique<StopMarketOrder>(symbolID, orderID, params.side, params.stopPrice, params.quantity, params.traderID);
        case OrderType::STOP_LIMIT:
            if (params.price == 0 || params.stopPrice == 0) {
                throw InvalidPriceException("Stop Limit orders must have a price and a stop price.");
            }
            return std::make_unique<StopLimitOrder>(symbolID, orderID, params.side, params.price, params.stopPrice, params.quantity, params.traderID);
        default:
            throw UnsupportedOrderTypeException("Unsupported order type.");
    }
}
