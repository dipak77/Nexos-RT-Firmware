idf_component_register(
    SRCS "gpio_hal.cpp" "spi_hal.cpp" "i2c_hal.cpp" "usb_hal.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_driver_spi
             esp_common common board
)
