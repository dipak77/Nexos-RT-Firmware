idf_component_register(
    SRCS "command_service.cpp" "command_registry.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common storage connectivity time_service display gui diagnostics ota board device_hal event_bus console esp_timer microkernel lvgl_adapter lvgl
)
