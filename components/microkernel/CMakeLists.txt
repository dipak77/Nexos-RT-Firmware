idf_component_register(
    SRCS
        "core/mk_kernel.c"
        "core/mk_task.c"
        "core/mk_scheduler.c"
        "core/mk_idle.c"
        "core/mk_diagnostics.c"
        "ipc/mk_mutex.c"
        "ipc/mk_semaphore.c"
        "ipc/mk_queue.c"
        "ipc/mk_event.c"
        "time/mk_tick.c"
        "time/mk_timer.c"
        "memory/mk_pool.c"
        "arch/esp32s3/mk_arch.c"
        "arch/esp32s3/mk_port_shim.c"
        "port/native/mk_port_native.c"
        "port/freertos/mk_port_freertos.c"
    INCLUDE_DIRS "include" "arch/esp32s3" "port" "port/native" "port/freertos"
    REQUIRES freertos esp_timer esp_common common heap esp_system
)

# App layer should only include mk/*.h, never freertos/*.h
