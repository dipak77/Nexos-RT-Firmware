#include "mk_health.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "MK_HEALTH";

static mk_health_snapshot_t s_snapshot = {
    .device_score = 100,
    .mem_score = 100,
    .crash_score = 100,
    .net_score = 100,
    .fault_code = "OK",
    .uptime_s = 0,
    .reboot_count = 0,
    .stall_count = 0,
    .mode = MK_HEALTH_MODE_NORMAL
};

static uint64_t s_last_recover_us = 0;
static uint64_t s_init_time_us = 0;

void mk_health_init(void){
    s_init_time_us = esp_timer_get_time();
    s_last_recover_us = s_init_time_us;
    s_snapshot.device_score = 100;
    s_snapshot.mem_score = 100;
    s_snapshot.crash_score = 100;
    s_snapshot.net_score = 100;
    strncpy(s_snapshot.fault_code, "OK", sizeof(s_snapshot.fault_code)-1);
    s_snapshot.mode = MK_HEALTH_MODE_NORMAL;
    ESP_LOGI(TAG, "Nexos-RT Health Management Subsystem initialized (score=100, mode=NORMAL)");
}

void mk_health_record_fault(const char* fault_code, uint8_t penalty){
    if(!fault_code) return;
    if(s_snapshot.device_score > penalty){
        s_snapshot.device_score -= penalty;
    } else {
        s_snapshot.device_score = 0;
    }

    if(strstr(fault_code, "STALL")){
        s_snapshot.stall_count++;
    }
    if(strstr(fault_code, "HEAP") || strstr(fault_code, "MEM")){
        if(s_snapshot.mem_score > penalty) s_snapshot.mem_score -= penalty;
        else s_snapshot.mem_score = 0;
    }
    if(strstr(fault_code, "NET") || strstr(fault_code, "WIFI")){
        if(s_snapshot.net_score > penalty) s_snapshot.net_score -= penalty;
        else s_snapshot.net_score = 0;
    }

    strncpy(s_snapshot.fault_code, fault_code, sizeof(s_snapshot.fault_code)-1);
    s_snapshot.fault_code[sizeof(s_snapshot.fault_code)-1] = '\0';

    // Update degradation policy
    if(s_snapshot.device_score < 40){
        s_snapshot.mode = MK_HEALTH_MODE_SAFE_MODE;
    } else if(s_snapshot.device_score < 60){
        s_snapshot.mode = MK_HEALTH_MODE_DEGRADED_NET;
    } else if(s_snapshot.device_score < 80){
        s_snapshot.mode = MK_HEALTH_MODE_DEGRADED_UI;
    }

    ESP_LOGW(TAG, "Health fault recorded: %s (score=%u, mode=%s)",
             s_snapshot.fault_code, s_snapshot.device_score, mk_health_mode_to_string(s_snapshot.mode));
}

void mk_health_tick(void){
    uint64_t now = esp_timer_get_time();
    s_snapshot.uptime_s = (uint32_t)((now - s_init_time_us) / 1000000ULL);

    // Score recovery: +1 point every 60 seconds healthy
    if(now - s_last_recover_us >= 60000000ULL){
        s_last_recover_us = now;
        if(s_snapshot.device_score < 100){
            s_snapshot.device_score++;
            if(s_snapshot.device_score >= 80){
                s_snapshot.mode = MK_HEALTH_MODE_NORMAL;
                strncpy(s_snapshot.fault_code, "OK", sizeof(s_snapshot.fault_code)-1);
            } else if(s_snapshot.device_score >= 60){
                s_snapshot.mode = MK_HEALTH_MODE_DEGRADED_UI;
            } else if(s_snapshot.device_score >= 40){
                s_snapshot.mode = MK_HEALTH_MODE_DEGRADED_NET;
            }
        }
    }
}

mk_health_snapshot_t mk_health_get_snapshot(void){ return s_snapshot; }
mk_health_mode_t mk_health_get_mode(void){ return s_snapshot.mode; }
void mk_health_set_mode(mk_health_mode_t mode){ s_snapshot.mode = mode; }
bool mk_health_is_healthy(void){ return s_snapshot.device_score >= 80; }
