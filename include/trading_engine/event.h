#pragma once
#include "types.h"
#include <string>
#include <sstream>

enum class EventType {
    ORDER_ACCEPTED,
    ORDER_REJECTED,
    ORDER_CANCELLED,
    TRADE_EXECUTED,
    // Add other event types as needed
};

struct Event {
    EventType type;
    virtual ~Event() = default;
    virtual std::string toString() const = 0;
};
