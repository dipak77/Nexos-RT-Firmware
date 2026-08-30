#include "event_bus.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "EVENT_BUS";

namespace smart_device {

EventBus& EventBus::instance(){ static EventBus inst; return inst; }

bool EventBus::initialize(){
    if(initialized_) return true;
    queue_ = mk_queue_create(64, sizeof(AppEvent), "event_bus");
    if(!queue_){ ESP_LOGE(TAG, "Queue create failed"); return false; }
    initialized_ = true;
    ESP_LOGI(TAG, "Event bus initialized");
    return true;
}

bool EventBus::publish(const AppEvent& ev){
    if(!queue_) return false;
    AppEvent e = ev;
    if(e.timestamp_ms==0) e.timestamp_ms = (uint32_t)mk_time_ms();
    if(mk_queue_send(queue_, &e, 10) != MK_OK){
        ESP_LOGW(TAG, "Event queue full");
        return false;
    }
    return true;
}

bool EventBus::subscribe(AppEventType type, EventCallback cb){
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.push_back({type, cb, false});
    return true;
}
bool EventBus::subscribe_all(EventCallback cb){
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.push_back({AppEventType::SYSTEM_BOOTED, cb, true});
    return true;
}

void EventBus::process_events_blocking(uint32_t timeout_ms){
    AppEvent ev{};
    if(mk_queue_receive(queue_, &ev, timeout_ms) == MK_OK){
        std::vector<Subscription> copy;
        { std::lock_guard<std::mutex> lock(mutex_); copy = subs_; }
        // Callbacks run in SYSTEM thread context — they must be non-blocking and must NOT
        // directly touch LVGL or other task-owned resources. Queue a UiCommand or publish
        // another event instead of locking the display here.
        for(auto &s: copy){
            if(s.all || s.type==ev.type){
                // Guard against callback throwing/panicking stalling SYSTEM watchdog
                s.cb(ev);
            }
        }
    }
}

} // namespace smart_device
