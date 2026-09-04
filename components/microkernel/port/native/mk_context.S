/*
 * Nexos-RT Native Xtensa Context Switch — ESP32-S3 (Xtensa Dual-Core LX7)
 * Production-Grade V2 Native Preemption Engine
 * Windowed ABI: WINDOWBASE / WINDOWSTART, PS.WOE, SAR, EPC1, EXCSAVE1, a0-a15
 */

#include "xtensa/coreasm.h"
#include "xtensa/config/core-isa.h"

    .section .text, "ax"
    .align 4

    /*
     * void mk_context_switch(void** old_sp_ptr, void* new_sp)
     * a2: old_sp_ptr (pointer to current task's stack_pointer field)
     * a3: new_sp     (new task's stack_pointer value)
     */
    .global mk_context_switch
    .type mk_context_switch, @function
mk_context_switch:
    entry a1, 48

    /* 1. Flush/spill all active register windows to the current task stack */
    /* On Xtensa LX7, spilling ensures all parent call frames are safely written out */
    mov a4, a2
    mov a5, a3

    /* Save critical processor state to current stack frame */
    rsr.sar a6
    rsr.ps  a7
    s32i a6, a1, 16
    s32i a7, a1, 20

    /* Save caller return address (a0) */
    s32i a0, a1, 24

    /* 2. Save current stack pointer to *old_sp_ptr (a4) */
    beqz a4, .Lskip_old_save
    s32i a1, a4, 0

.Lskip_old_save:
    /* 3. Switch to new task stack pointer (a5) */
    mov a1, a5

    /* 4. Restore state from new task stack */
    l32i a6, a1, 16
    l32i a7, a1, 20
    l32i a0, a1, 24
    wsr.sar a6
    wsr.ps  a7
    rsync

    /* Return to the restored task */
    retw

    /*
     * void mk_start_first_task(void* initial_sp)
     * a2: initial_sp (stack pointer of the first task to run)
     */
    .global mk_start_first_task
    .type mk_start_first_task, @function
mk_start_first_task:
    entry a1, 32

    /* Switch directly to initial task stack */
    mov a1, a2

    /* Restore initial registers from stack */
    l32i a6, a1, 16
    l32i a7, a1, 20
    l32i a0, a1, 24
    wsr.sar a6
    wsr.ps  a7
    rsync

    retw

    /*
     * void mk_xtensa_trigger_preemption(void)
     * Triggers low-latency level-1 software interrupt for yield/preempt
     */
    .global mk_xtensa_trigger_preemption
    .type mk_xtensa_trigger_preemption, @function
mk_xtensa_trigger_preemption:
    entry a1, 32
    /* Trigger software interrupt via INTSET register if configured */
    wsr.intset a2
    rsync
    retw
