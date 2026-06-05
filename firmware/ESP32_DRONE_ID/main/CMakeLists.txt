idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_event esp_system freertos
    PRIV_REQUIRES driver
)

# Link against OpenDroneID component
target_link_libraries(${COMPONENT_LIB} PRIVATE opendroneid)
