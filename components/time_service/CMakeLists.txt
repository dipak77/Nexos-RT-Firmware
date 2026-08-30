idf_component_register(
    SRCS "time_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common storage event_bus esp_netif
)
