#pragma once
// Legacy FreeRTOS shim definitions — ONLY included when MK_NATIVE_KERNEL==0
// Kept isolated so core never sees freertos/*.h when native is enabled.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
