idf_component_register(SRCS "temp_server.c" "main.c"
                            "sht40_front.c"
                            "WiFi.c" "btn.c" "send_data_ws.c" "day_night_logic.c"
                    INCLUDE_DIRS "include")

spiffs_create_partition_image(web ../site/build FLASH_IN_PROJECT)