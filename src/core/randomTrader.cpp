#include "trading_engine/randomTrader.h"
#include <iostream>

RandomTrader::RandomTrader(MatchingEngine& engine, EventDispatcher& dispatcher, const std::string& symbol, std::chrono::milliseconds interval)
    : Trader(engine, dispatcher),
      symbol_id_(SymbolRegistry::getInstance().getID(symbol)),
      interval_(interval),
      rng_(std::random_device{}()) {
    last_tick_ = std::chrono::steady_clock::now();
}

void RandomTrader::tick() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_tick_ < interval_) {
        return;
    }
    last_tick_ = now;

    // Generate random order
    Side side = side_dist_(rng_) == 0 ? Side::BUY : Side::SELL;
    Price price = static_cast<Price>(price_dist_(rng_));
    Quantity quantity = static_cast<Quantity>(quantity_dist_(rng_));

    RawOrderParams params = {
        .symbol = SymbolRegistry::getInstance().getSymbol(symbol_id_),
        .traderID = 100, // Example trader ID
        .side = side,
        .orderType = OrderType::LIMIT,
        .quantity = quantity,
        .price = price,
        .tif = TimeInForce::GTC
    };

    try {
        OrderID orderID = engine_.submitOrder(params);
        std::cout << "RandomTrader submitted order: " << orderID << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "RandomTrader failed to submit order: " << e.what() << std::endl;
    }
}
