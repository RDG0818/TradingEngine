#include "limitOrder.h"

inline bool isValidPrice(const str& priceStr) {
    if (priceStr.find('-') != std::string::npos) return false;
    auto dot = priceStr.find('.');
    if (dot == std::string::npos) return false; 
    return (priceStr.size() - dot - 1) == 2; 
}

inline Price convertPriceToInt(const std::string& priceStr) {
    str tempStr = priceStr;
    tempStr.erase(std::remove(tempStr.begin(), tempStr.end(), '.'), tempStr.end());
    unsigned int priceInt = std::stoul(tempStr);
    return priceInt;
}

LimitOrder::LimitOrder(OrderID orderID, OrderType orderType, Side side, str priceStr, Quantity quantity, TraderID traderID) {
   if (!isValidPrice(priceStr)) {
    throw std::invalid_argument("Price must be have at most 2 decimal places and be positive. (Ex. 10.00, 123.45)");
   }
   if (quantity == 0) {
    throw std::invalid_argument("Quantity must be positive.");
   }
   setPriceStr(priceStr); setPrice(convertPriceToInt(priceStr));
   setOrderID(orderID); setOrderType(orderType); setSide(side); setQuantity(quantity); setTraderID(traderID);
}

bool LimitOrder::isMarketOrder() const {
    return false;
}

