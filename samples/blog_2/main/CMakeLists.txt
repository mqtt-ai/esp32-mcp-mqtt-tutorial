idf_component_register(SRCS "main.c" "wifi.c"
                    REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
