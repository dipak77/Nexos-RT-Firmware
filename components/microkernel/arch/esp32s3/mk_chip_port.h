#pragma once
// Internal ESP32-S3 chip-support for leftover core files.
// When MK_NATIVE_KERNEL==1 this must NOT include freertos/*.h and must NOT
// stub task APIs (NULL current-task / no-op delete). Critical sections go
// through mk_port_enter_critical() which uses a real spinlock in the port .c.
#include "mk_config.h"

#if MK_NATIVE_KERNEL
#include "mk_port.h"
#ifndef taskENTER_CRITICAL
#define taskENTER_CRITICAL(mux) do { (void)(mux); mk_port_enter_critical(); } while (0)
#endif
#ifndef taskEXIT_CRITICAL
#define taskEXIT_CRITICAL(mux)  do { (void)(mux); mk_port_exit_critical(); } while (0)
#endif
#ifndef configMAX_PRIORITIES
#define configMAX_PRIORITIES 25
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY 0xFFFFFFFFu
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif
#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#ifndef pdPASS
#define pdPASS 1
#endif
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#endif

