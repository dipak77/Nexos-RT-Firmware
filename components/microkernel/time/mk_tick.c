#include "mk_timer.h"
#include "esp_timer.h"
uint64_t mk_clock_get_ms(void){ return esp_timer_get_time()/1000; }
