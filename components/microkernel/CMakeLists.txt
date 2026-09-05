idf_component_register(
    SRCS
        "core/mk_kernel.c"
        "core/mk_task.c"
        "core/mk_scheduler.c"
        "core/mk_idle.c"
        "core/mk_diagnostics.c"
        "core/mk_health.c"
        "core/mk_crash.c"
        "core/mk_enclave.c"
        "core/mk_svc.c"
        "core/mk_fault.c"
        "ipc/mk_mutex.c"
        "ipc/mk_semaphore.c"
        "ipc/mk_queue.c"
        "ipc/mk_event.c"
        "time/mk_tick.c"
        "time/mk_timer.c"
        "memory/mk_pool.c"
        "arch/esp32s3/mk_arch.c"
        "arch/esp32s3/mk_port_shim.c"
        "arch/esp32s3/mk_gptimer.c"
        "arch/esp32s3/mk_ipi.c"
        "arch/esp32s3/mk_watchpoint.c"
        "port/native/mk_port_native.c"
        # port/native/mk_context.S EXCLUDED until rewritten against the Xtensa
        # windowed ABI with a loopback test. The current stub saves neither
        # WINDOWBASE/WINDOWSTART nor EPC/EXCSAVE, is never called, and is not
        # assembled in the Arduino build. See docs/REDESIGNED_NEXOS_RT_V2_SDK_ARCHITECTURE.md.
        "port/freertos/mk_port_freertos.c"
    INCLUDE_DIRS "include" "arch/esp32s3" "port" "port/native" "port/freertos"
    REQUIRES freertos esp_timer esp_common common heap esp_system esp_ipc
)

# App layer should only include mk/*.h, never freertos/*.h
