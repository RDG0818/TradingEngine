#include "marketOrder.h"

MarketOrder::MarketOrder(OrderID orderID, OrderType orderType, Side side, Quantity quantity, TraderID traderID) {
    if (quantity == 0) {
        throw std::invalid_argument("Quantity must be positive.");
    }
    setOrderID(orderID); setOrderType(orderType); setSide(side); 
    setQuantity(quantity); setTraderID(traderID);
};

bool MarketOrder::isMarketOrder() const {
    return true;
}


