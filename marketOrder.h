#pragma once
#include "order.h"
#include "types.h"

class MarketOrder : public Order {
    private:

    public:
        MarketOrder(
            OrderID orderID,
            OrderType orderType,
            Side side,
            Quantity quantity,
            TraderID traderID
        );
        
        bool isMarketOrder() const override;

};
