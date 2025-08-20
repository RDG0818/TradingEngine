#include "marketOrder.h"

MarketOrder::MarketOrder(str symbol, OrderID orderID, OrderType orderType, Side side, Quantity quantity, TraderID traderID) {
    if (quantity == 0) {
        throw std::invalid_argument("Quantity must be positive.");
    }
    setOrderID(orderID); setOrderType(orderType); setSide(side); 
    setSymbol(symbol); setQuantity(quantity); setTraderID(traderID);
};
