#pragma once
#include "common/result.h"
#include <ctime>
#include <sys/time.h>

namespace smart_device {
namespace time_service {

enum class TimeSource {
    NONE,
    SYSTEM,
    SNTP,
    RTC, // DS3231 future
    MANUAL
};

struct TimeStatus {
    bool synced{false};
    TimeSource source{TimeSource::NONE};
    time_t last_sync{0};
    char timezone[32]{"IST-5:30"};
};

class TimeService {
public:
    static TimeService& instance();
    Result<void> initialize(const char* timezone = "IST-5:30");
    Result<void> start_sntp();
    Result<void> stop_sntp();
    Result<void> set_timezone(const char* tz);
    Result<void> set_time_manual(time_t t);

    bool is_synced() const { return status_.synced; }
    TimeStatus status() const { return status_; }
    bool get_local_time(struct tm& out);
    void get_formatted(char* time_buf, size_t time_len, char* date_buf, size_t date_len, bool use_24h = false, char* ampm_buf = nullptr, size_t ampm_len = 0);

private:
    TimeService() = default;
    TimeStatus status_{};
    bool sntp_started_{false};
    static void sntp_sync_cb(::timeval* tv);
};

} // namespace time_service
} // namespace smart_device
