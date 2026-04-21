idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        wifi
        ota
    PRIV_REQUIRES
        nvs_flash
        esp_timer
)
