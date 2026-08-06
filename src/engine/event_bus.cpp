#include "engine/event_bus.h"
#include <algorithm>

void EventBus::unsubscribe(SubscriptionToken token) {
    std::unique_lock lock(mutex_);
    for (auto& [type, handlers] : handlers_) {
        auto it = std::find_if(handlers.begin(), handlers.end(),
            [token](const Handler& h) { return h.token == token; });
        if (it != handlers.end()) {
            handlers.erase(it);
            return;
        }
    }
}
