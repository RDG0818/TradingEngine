// include/eventDispatcher.h

#ifndef TRADINGENGINE_INCLUDE_EVENTDISPATCHER_H_
#define TRADINGENGINE_INCLUDE_EVENTDISPATCHER_H_

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "events.h" 

// TODO: Implement an unsubscribe mechanism, likely by having subscribe()
//               return a handle/ID that can be used to remove the callback.
// TODO: Consider using Boost.Signals2
class EventDispatcher {
private:
  mutable std::shared_mutex mtx_;
  std::unordered_map<std::type_index, std::vector<std::function<void(const BaseEvent&)>>> subscribers_;

public:
  template<typename TEvent>
  void subscribe(std::function<void(const TEvent&)> callback) {
    static_assert(std::is_base_of<BaseEvent, TEvent>::value, 
                  "TEvent must be a derivative of BaseEvent.");

    auto type_index = std::type_index(typeid(TEvent));

    auto wrapper_callback = [callback](const BaseEvent& event) {
      if (const auto* derived = dynamic_cast<const TEvent*>(&event)) {
        callback(*derived);
      }
    };

    std::unique_lock<std::shared_mutex> lock(mtx_);
    subscribers_[type_index].push_back(wrapper_callback);
  }

  void publish(const BaseEvent& event) {
    auto type_index = std::type_index(typeid(event));
    std::vector<std::function<void(const BaseEvent&)>> callbacks_to_run;

    {
      std::shared_lock<std::shared_mutex> lock(mtx_);
      auto it = subscribers_.find(type_index);
      if (it != subscribers_.end()) {
        callbacks_to_run = it->second;
      }
    }

    for (const auto& callback : callbacks_to_run) {
      try {
        callback(event);
      }
      catch (const std::bad_cast& e) {
        std::cerr << "Exception in event subscriber (bad_cast): " << e.what() << std::endl;
      }
      catch (const std::exception& e) {
        std::cerr << "Exception in event subscriber: " << e.what() << std::endl;
      }
    }
  }
};

#endif // TRADINGENGINE_INCLUDE_EVENTDISPATCHER_H_

