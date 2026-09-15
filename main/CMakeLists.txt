idf_component_register(
    SRCS "ota_node.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_https_ota
        bootloader_support
        app_update
        esp_netif
        esp_event
        nvs_flash
        esp_wifi
        esp_http_client
)