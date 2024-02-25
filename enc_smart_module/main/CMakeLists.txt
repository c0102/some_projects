# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(SRCS "main.c"
                            "WiFi.c"
                            "btn.c"
                    INCLUDE_DIRS ".")


spiffs_create_partition_image(spiffs ../site/build FLASH_IN_PROJECT)   

#idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")

