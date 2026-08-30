idf_component_register(
    SRCS "wifi_service.cpp" "ble_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common storage event_bus esp_wifi esp_event esp_netif bt nvs_flash microkernel
)
