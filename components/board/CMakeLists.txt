idf_component_register(
    SRCS "board.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common esp_system spi_flash esp_psram
)
