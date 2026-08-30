#include "mk_timer.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char* TAG = "MK_TICK";
uint64_t mk_clock_get_ms(void){ return esp_timer_get_time()/1000; }
