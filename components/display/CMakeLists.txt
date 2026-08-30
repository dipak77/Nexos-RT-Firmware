idf_component_register(
    SRCS "display.cpp" "display_gc9a01.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common board device_hal esp_lcd esp_lcd_gc9a01 microkernel driver heap
)
