idf_component_register(SRCS "FO_http_server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES FO_common nvs_flash esp_http_server spiffs mdns json app_update )
