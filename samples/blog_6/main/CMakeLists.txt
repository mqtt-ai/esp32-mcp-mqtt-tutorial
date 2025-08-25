idf_component_register(SRCS "radio.c" "main.c" "wifi.c" "send_image.c"
                    REQUIRES esp_wifi nvs_flash driver 
                    INCLUDE_DIRS ".")
