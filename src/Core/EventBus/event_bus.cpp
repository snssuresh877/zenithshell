#include "Core/EventBus/event_bus.hpp"

namespace zenith {

EventBus::EventBus() = default;

EventBus::~EventBus() = default;

bool EventBus::unsubscribe(SubscriptionId id) {
    if (id == INVALID_SUBSCRIPTION_ID) return false;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto& [type, list] : subscribers_) {
        if (list && list->unsubscribe(id)) {
            return true;
        }
    }
    return false;
}

void EventBus::clear() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    subscribers_.clear();
}

} // namespace zenith
