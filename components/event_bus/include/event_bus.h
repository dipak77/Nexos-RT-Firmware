#pragma once
#include "app_event.h"
#include "mk.h"
#include <functional>
#include <vector>
#include <mutex>

namespace smart_device {

using EventCallback = std::function<void(const AppEvent&)>;

class EventBus {
public:
    static EventBus& instance();
    bool initialize();
    bool publish(const AppEvent& event);
    bool subscribe(AppEventType type, EventCallback cb);
    bool subscribe_all(EventCallback cb);
    void process_events_blocking(uint32_t timeout_ms = 100);

private:
    mk_queue_t* queue_{nullptr};
    struct Subscription {
        AppEventType type;
        EventCallback cb;
        bool all{false};
    };
    std::vector<Subscription> subs_;
    std::mutex mutex_;
    bool initialized_{false};
};

} // namespace smart_device
