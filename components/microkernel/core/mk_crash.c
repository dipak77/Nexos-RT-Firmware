#include "mk_crash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <string.h>

static const char* TAG = "MK_CRASH";

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32)
RTC_NOINIT_ATTR static mk_crash_record_t s_rtc_crash_record;
RTC_NOINIT_ATTR static uint32_t s_rtc_reboot_counter;
#else
static mk_crash_record_t s_rtc_crash_record;
static uint32_t s_rtc_reboot_counter = 0;
#endif

static bool s_has_crash_record = false;

void mk_crash_init(void){
    if(s_rtc_crash_record.magic == MK_CRASH_MAGIC){
        s_has_crash_record = true;
        // RTC_NOINIT retains across soft reboot but is random on first power-on;
        // only trust counter if magic was already valid (it is here).
        s_rtc_reboot_counter++;
        s_rtc_crash_record.reboot_count = s_rtc_reboot_counter;
        ESP_LOGE(TAG, "[CRASH FLIGHT RECORDER] Previous fault retained: cause=%s task=%s pc=0x%08lx heap=%luB uptime=%lus reboots=%lu (clear with fault clear)",
                 s_rtc_crash_record.cause,
                 s_rtc_crash_record.task_name,
                 (unsigned long)s_rtc_crash_record.fault_pc,
                 (unsigned long)s_rtc_crash_record.free_heap_at_fault,
                 (unsigned long)s_rtc_crash_record.uptime_s,
                 (unsigned long)s_rtc_crash_record.reboot_count);
    } else {
        s_has_crash_record = false;
        memset(&s_rtc_crash_record, 0, sizeof(s_rtc_crash_record));
        // First power-on: RTC_NOINIT is indeterminate, force deterministic baseline.
        s_rtc_reboot_counter = 0;
    }
}

void mk_crash_save(const char* cause, const char* task_name, uint32_t fault_pc, const char* trace){
    s_rtc_crash_record.magic = MK_CRASH_MAGIC;
    strncpy(s_rtc_crash_record.cause, cause ? cause : "UNKNOWN", sizeof(s_rtc_crash_record.cause)-1);
    strncpy(s_rtc_crash_record.task_name, task_name ? task_name : "UNKNOWN", sizeof(s_rtc_crash_record.task_name)-1);
    s_rtc_crash_record.fault_pc = fault_pc;
    s_rtc_crash_record.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    s_rtc_crash_record.free_heap_at_fault = (uint32_t)esp_get_free_heap_size();
    if(trace) strncpy(s_rtc_crash_record.last_trace, trace, sizeof(s_rtc_crash_record.last_trace)-1);
    s_has_crash_record = true;
}

bool mk_crash_has_record(void){ return s_has_crash_record; }

bool mk_crash_get_record(mk_crash_record_t* record){
    if(!record || !s_has_crash_record) return false;
    memcpy(record, &s_rtc_crash_record, sizeof(*record));
    return true;
}

void mk_crash_clear(void){
    s_has_crash_record = false;
    memset(&s_rtc_crash_record, 0, sizeof(s_rtc_crash_record));
}
