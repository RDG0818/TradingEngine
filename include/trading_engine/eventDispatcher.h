#pragma once
#include <vector>
#include <unordered_map>
#include <functional>
#include "event.h"

class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void onEvent(const Event& event) = 0;
};

class EventDispatcher {
public:
    void subscribe(EventType type, EventListener* listener);
    void publish(const Event& event);

private:
    std::unordered_map<EventType, std::vector<EventListener*>> listeners;
};