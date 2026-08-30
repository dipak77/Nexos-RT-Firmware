#include "mk_kernel.h"
#include "esp_log.h"
static const char* TAG = "MK_IDLE";
void mk_idle_task(void* arg){
    while(true){
        // In real kernel: WFI, tickless check, run garbage collector
        mk_sleep_ms(100);
    }
}
