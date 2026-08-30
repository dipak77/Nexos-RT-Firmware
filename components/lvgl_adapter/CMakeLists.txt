idf_component_register(
    SRCS "lvgl_adapter.cpp" "lvgl_tick.c"
    INCLUDE_DIRS "include"
    REQUIRES microkernel display board lvgl esp_lcd heap
)
