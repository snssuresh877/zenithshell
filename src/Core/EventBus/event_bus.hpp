#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace zenith {

using SubscriptionId = uint64_t;
constexpr SubscriptionId INVALID_SUBSCRIPTION_ID = 0;

namespace detail {

class ISubscriberList {
public:
    virtual ~ISubscriberList() = default;
    virtual bool unsubscribe(SubscriptionId id) = 0;
    virtual size_t size() const = 0;
};

template <typename Event>
class SubscriberList : public ISubscriberList {
public:
    using Callback = std::function<void(const Event&)>;

    struct Entry {
        SubscriptionId id;
        Callback callback;
    };

    SubscriptionId subscribe(SubscriptionId id, Callback cb) {
        entries.push_back({id, std::move(cb)});
        return id;
    }

    bool unsubscribe(SubscriptionId id) override {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->id == id) {
                entries.erase(it);
                return true;
            }
        }
        return false;
    }

    void publish(const Event& event) {
        if (entries.empty()) return;

        // Snapshot copy: ensures iteration is never invalidated
        // if callbacks subscribe or unsubscribe during execution.
        auto snapshot = entries;

        for (const auto& entry : snapshot) {
            // Verify listener was not unsubscribed earlier during this dispatch
            if (is_active(entry.id)) {
                entry.callback(event);
            }
        }
    }

    size_t size() const override {
        return entries.size();
    }

private:
    bool is_active(SubscriptionId id) const {
        for (const auto& entry : entries) {
            if (entry.id == id) return true;
        }
        return false;
    }

    std::vector<Entry> entries;
};

} // namespace detail

class EventBus {
public:
    EventBus();
    ~EventBus();

    // Non-copyable, movable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;

    template <typename Event>
    SubscriptionId subscribe(std::function<void(const Event&)> callback) {
        if (!callback) return INVALID_SUBSCRIPTION_ID;

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        SubscriptionId id = next_id_++;
        auto& list = get_or_create_list<Event>();
        return list.subscribe(id, std::move(callback));
    }

    template <typename Event>
    bool unsubscribe(SubscriptionId id) {
        if (id == INVALID_SUBSCRIPTION_ID) return false;

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(Event)));
        if (it != subscribers_.end()) {
            return it->second->unsubscribe(id);
        }
        return false;
    }

    bool unsubscribe(SubscriptionId id);

    template <typename Event>
    void publish(const Event& event) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(Event)));
        if (it != subscribers_.end()) {
            auto* list = static_cast<detail::SubscriberList<Event>*>(it->second.get());
            list->publish(event);
        }
    }

    template <typename Event>
    size_t subscriber_count() const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(Event)));
        if (it != subscribers_.end()) {
            return it->second->size();
        }
        return 0;
    }

    void clear();

private:
    template <typename Event>
    detail::SubscriberList<Event>& get_or_create_list() {
        auto key = std::type_index(typeid(Event));
        auto it = subscribers_.find(key);
        if (it == subscribers_.end()) {
            auto list = std::make_unique<detail::SubscriberList<Event>>();
            auto* ptr = list.get();
            subscribers_[key] = std::move(list);
            return *ptr;
        }
        return *static_cast<detail::SubscriberList<Event>*>(it->second.get());
    }

    mutable std::recursive_mutex mutex_;
    SubscriptionId next_id_ = 1;
    std::unordered_map<std::type_index, std::unique_ptr<detail::ISubscriberList>> subscribers_;
};

} // namespace zenith
