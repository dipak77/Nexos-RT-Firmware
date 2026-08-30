idf_component_register(
    SRCS "nvs_store.cpp" "settings_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common nvs_flash
)
