// include/orderFactory.h

#ifndef TRADINGENGINE_INCLUDE_ORDERFACTORY_H_
#define TRADINGENGINE_INCLUDE_ORDERFACTORY_H_

#include <memory>
#include <string>

#include "events.h"
#include "order.h"
#include "utils.h"

// TODO: Do resource pooling

struct RawOrderParams {
    std::string symbol;
    OrderType order_type;
    Side side;
    std::string price;
    std::string stop_price;
    Quantity quantity;
    TraderID trader_id;
    TimeInForce time_in_force = TimeInForce::GTC;
};

namespace order_factory {

std::shared_ptr<Order> create_order(const RawOrderParams& params, OrderID order_id);

} // namespace order_factory

#endif // TRADINGENGINE_INCLUDE_ORDERFACTORY_H_
