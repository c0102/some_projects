idf_component_register(SRCS "server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES enchele_app_helpers esp_http_server message_parsing spiffs )
                    
#spiffs_create_partition_image(spiffs ../site/build FLASH_IN_PROJECT)  