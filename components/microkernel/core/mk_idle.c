#include "mk_kernel.h"
#include "mk_task.h"
#include "mk_port.h"

void mk_idle_task(void* arg){
    (void)arg;
    while(true){
#if defined(__XTENSA__)
        // Hardware Wait-for-Interrupt low-power standby on Xtensa LX7
        __asm__ volatile ("waiti 0");
#else
        mk_sleep_ms(10);
#endif
        mk_task_reap();
        mk_yield();
    }
}
