#include "trading_engine/trader.h"

Trader::Trader(MatchingEngine& engine, EventDispatcher& dispatcher) : engine_(engine), dispatcher_(dispatcher) {}
