idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES board device_hal display gui command connectivity time_service storage diagnostics ota event_bus common microkernel lvgl_adapter platform lvgl esp_lcd nvs_flash freertos
)
