idf_component_register(
    SRCS "gui.cpp" "gui_dashboard.cpp" "gui_status.cpp" "gui_command.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common board display event_bus connectivity time_service diagnostics storage lvgl microkernel lvgl_adapter esp_lcd
)
