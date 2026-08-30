idf_component_register(
    SRCS "diagnostics.cpp" "health_monitor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common board device_hal display connectivity time_service storage event_bus microkernel
)
