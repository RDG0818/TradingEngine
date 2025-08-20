#pragma once
#include "order.h"

class LimitOrder : public Order {
    private:
        str priceStr;
        Price price;
    public:
        LimitOrder(
            str symbol,
            OrderID orderID,
            OrderType orderType,
            Side side,
            str priceStr,
            Quantity quantity,
            TraderID traderID
        );

        str getPriceStr() {
            return priceStr;
        }
        void setPriceStr(str priceStr) {
            this->priceStr = priceStr;
        }
        Price getPrice() {
            return price;
        }
        void setPrice(Price price) {
            this->price = price;
        }
        
};