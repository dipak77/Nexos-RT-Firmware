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
#include "mk_enclave.h"
#include "mk_svc.h"
#include "mk_fault.h"

// Optional C++ wrappers
#ifdef __cplusplus
#include "mk_cpp.hpp"
#endif

// Do not reject a translation unit merely because an ESP-IDF header pulled in
// FreeRTOS transitively.  The preprocessor cannot distinguish that legitimate
// dependency from a direct application include, and the former made ordinary
// Wi-Fi/console users fail depending on include order.  Direct vendor scheduler
// includes are enforced by the source-contract tests instead.
