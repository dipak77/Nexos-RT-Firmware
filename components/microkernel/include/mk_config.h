#pragma once
// Nexos-RT configuration
#define MK_CONFIG_OS_NAME "Nexos-RT"
#define MK_CONFIG_OS_NAME_U "NEXOS-RT"
#define MK_CONFIG_VERSION_MAJOR 1
#define MK_CONFIG_VERSION_MINOR 1
#define MK_CONFIG_VERSION_PATCH 0
#define MK_CONFIG_VERSION_STRING "1.1.0"

#define MK_CONFIG_TICK_HZ 1000  // 1ms tick
#define MK_CONFIG_MAX_TASKS 16
#define MK_CONFIG_MAX_PRIORITIES 8
#define MK_CONFIG_IDLE_STACK 2048
#define MK_CONFIG_USE_PREEMPTION 1
#define MK_CONFIG_USE_ROUND_ROBIN 1
#define MK_CONFIG_USE_MUTEX_PI 1  // priority inheritance - critical for production
#define MK_CONFIG_USE_STATS 1
#define MK_CONFIG_USE_STACK_WATERMARK 1
#define MK_CONFIG_CPU_AFFINITY_SINGLE_CORE 0

// Priority map (higher number = higher priority)
#define MK_PRIO_IDLE 1
#define MK_PRIO_STORAGE 2
#define MK_PRIO_DIAGNOSTICS 3
#define MK_PRIO_TIME 4
#define MK_PRIO_CONNECTIVITY 5
#define MK_PRIO_COMMAND 6
#define MK_PRIO_GUI 7
#define MK_PRIO_HIGHEST 7

// Stack sizes in bytes (ESP-IDF task create units)
#define MK_STACK_GUI 12288
#define MK_STACK_COMMAND 8192
#define MK_STACK_CONNECTIVITY 8192
#define MK_STACK_TIME 4096
#define MK_STACK_DIAGNOSTICS 4096
#define MK_STACK_STORAGE 4096

#define MK_WATCHDOG_TIMEOUT_MS 10000
