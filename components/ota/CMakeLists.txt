idf_component_register(
    SRCS "ota_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common event_bus esp_https_ota app_update esp_http_client mbedtls microkernel
)
