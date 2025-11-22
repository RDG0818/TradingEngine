#pragma once
#include "matchingEngine.h"
#include "eventDispatcher.h"

class Trader {
public:
    Trader(MatchingEngine& engine, EventDispatcher& dispatcher);
    virtual ~Trader() = default;

    virtual void tick() = 0;

protected:
    MatchingEngine& engine_;
    EventDispatcher& dispatcher_;
};
