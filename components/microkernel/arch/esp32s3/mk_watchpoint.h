#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Arm hardware data watchpoint on the bottom 32 bytes of a task stack.
 * If stack overflow occurs, a hardware debug trap is triggered instantly.
 * @param watchpoint_id 0 or 1 (ESP32-S3 has 2 hardware watchpoints)
 * @param stack_base Pointer to the base (lowest address) of the stack buffer
 * @param size Number of bytes to guard (e.g. 32 bytes)
 * @return true if armed, false otherwise
 */
bool mk_watchpoint_arm_stack_guard(int watchpoint_id, const void* stack_base, size_t size);

/**
 * @brief Disarm the specified hardware watchpoint.
 */
void mk_watchpoint_disarm(int watchpoint_id);

#ifdef __cplusplus
}
#endif
