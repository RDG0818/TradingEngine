#include "trading_engine/eventDispatcher.h"
#include <iostream>

void EventDispatcher::subscribe(EventType type, EventListener* listener) {
    listeners[type].push_back(listener);
}

void EventDispatcher::publish(const Event& event) {
    if (listeners.count(event.type)) {
        for (auto listener : listeners[event.type]) {
            try {
                listener->onEvent(event);
            } catch (const std::exception& e) {
                std::cerr << "Exception in listener: " << e.what() << std::endl;
            }
        }
    }
}
