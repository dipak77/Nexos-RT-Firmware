/* Nexos-RT Native Xtensa Context Switch — ESP32-S3 LX7
 * Phase 2: replaces hidden FreeRTOS shim in mk_port_native.c
 * This stub reserves the binary layout; full save/restore is next commit.
 * Windowed ABI: WINDOWBASE/WINDOWSTART, PS.WOE, SAR, EPC, EXCSAVE, a0-a15
 * Tick: GPTimer 1kHz → PendSV (software interrupt) → mk_scheduler_pick_next
 */

#include "xtensa/coreasm.h"
#include "xtensa/config/core-isa.h"

    .section .text, "ax"
    .align 4
    .global mk_context_switch
    .type mk_context_switch, @function
mk_context_switch:
    /* Save caller window */
    entry a1, 32
    /* TODO: spill a0-a15, SAR, PS, EPC1, EXCSAVE1, WINDOWBASE, WINDOWSTART to TCB.stack_pointer */
    /* For now just yield — shim still uses FreeRTOS */
    retw

    .global mk_start_first_task
    .type mk_start_first_task, @function
mk_start_first_task:
    entry a1, 32
    /* TODO: restore first TCB and eret */
    retw

    .global SysTick_Handler
    .type SysTick_Handler, @function
SysTick_Handler:
    entry a1, 32
    /* TODO: increment tick, set PendSV */
    retw
