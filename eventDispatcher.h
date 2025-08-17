#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <typeindex>
#include <iostream>
#include <mutex>


class EventDispatcher {
private:
    std::mutex mtx;
    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> subscribers;

public:
    template<typename TEvent>
    void subscribe(std::function<void(const TEvent&)> callback) {
        std::lock_guard<std::mutex> lock(mtx);
        auto typeIndex = std::type_index(typeid(TEvent));

        auto wrapper = [callback](const std::any& event) {
            callback(std::any_cast<const TEvent&>(event));
        };

        subscribers[typeIndex].push_back(wrapper);
    }

   template<typename TEvent>
    void publish(const TEvent& event) {
        auto typeIndex = std::type_index(typeid(TEvent));
        std::vector<std::function<void(const std::any&)>> callbacks;

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (subscribers.count(typeIndex)) {
                callbacks = subscribers.at(typeIndex);
            }
        }
        for (const auto& callback : callbacks) {
           try {callback(event);}
           catch (const std::exception& e) {std::cerr << "Exception in event subscriber: " << e.what() << std::endl;}
        }
    }
};

