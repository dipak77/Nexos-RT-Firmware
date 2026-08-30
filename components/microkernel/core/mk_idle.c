#include "mk_kernel.h"
void mk_idle_task(void* arg){
    while(true){
        // In real kernel: WFI, tickless check, run garbage collector
        mk_sleep_ms(100);
    }
}
