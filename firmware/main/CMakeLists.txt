idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        wifi
        ota
        partition_scan
    PRIV_REQUIRES
        nvs_flash
        esp_timer
)
