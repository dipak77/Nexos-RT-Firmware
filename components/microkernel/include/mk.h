#pragma once
// Nexos-RT public API. Application / SDK / GUI / services include this only.

#include "mk_config.h"
#include "mk_types.h"
#include "mk_kernel.h"
#include "mk_task.h"
#include "mk_queue.h"
#include "mk_mutex.h"
#include "mk_semaphore.h"
#include "mk_event.h"
#include "mk_timer.h"
#include "mk_memory.h"
#include "mk_diagnostics.h"

// Optional C++ wrappers
#ifdef __cplusplus
#include "mk_cpp.hpp"
#endif

#if defined(INC_FREERTOS_H) && !defined(MK_ALLOW_CHIP_PORT)
#error "Vendor scheduler headers are not allowed in application code. Include mk.h and use mk_*."
#endif
