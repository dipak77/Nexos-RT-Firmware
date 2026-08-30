idf_component_register(
    SRCS "esp32s3_platform.cpp"
    INCLUDE_DIRS "include"
    REQUIRES microkernel driver esp_wifi esp_event esp_netif bt
)
