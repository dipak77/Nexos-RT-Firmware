#include "mk_scheduler.h"
#include "esp_log.h"

static const char* TAG = "MK_SCHED";
static uint64_t s_context_switches = 0;
static mk_task_t* s_ready_queues[MK_CONFIG_MAX_PRIORITIES];

void mk_scheduler_init(void){
    for(int i=0;i<MK_CONFIG_MAX_PRIORITIES;i++) s_ready_queues[i]=NULL;
    s_context_switches=0;
    ESP_LOGI(TAG, "Scheduler init prio=%d max_tasks=%d tick=%dHz", MK_CONFIG_MAX_PRIORITIES, MK_CONFIG_MAX_TASKS, MK_CONFIG_TICK_HZ);
}
void mk_scheduler_tick(uint64_t now_ms){
    // Wake sleeping tasks and update stats.
    s_context_switches++;
}
mk_task_t* mk_scheduler_pick_next(void){
    // Find highest priority non-empty ready queue
    for(int prio=MK_CONFIG_MAX_PRIORITIES-1; prio>=0; --prio){
        if(s_ready_queues[prio]) return s_ready_queues[prio];
    }
    return NULL;
}
void mk_scheduler_add_ready(mk_task_t* task){
    if(!task) return;
    int prio = task->priority;
    if(prio<0||prio>=MK_CONFIG_MAX_PRIORITIES) prio=1;
    task->next = s_ready_queues[prio];
    s_ready_queues[prio]=task;
}
void mk_scheduler_remove_ready(mk_task_t* task){
    if(!task) return;
    int prio = task->priority;
    if(prio<0||prio>=MK_CONFIG_MAX_PRIORITIES) return;
    mk_task_t** cur = &s_ready_queues[prio];
    while(*cur){
        if(*cur==task){ *cur=task->next; task->next=NULL; break; }
        cur=&(*cur)->next;
    }
}
uint64_t mk_scheduler_get_context_switches(void){ return s_context_switches; }
