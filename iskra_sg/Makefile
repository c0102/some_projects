#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

include sdkconfig
include version.txt

#PROJECT_NAME := ethernet_board
ifdef CONFIG_BOARD_SG
PROJECT_NAME := SG-$(VERSION)
else ifdef CONFIG_BOARD_IE14MW
PROJECT_NAME := IE14MW-$(VERSION)
else ifdef CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
PROJECT_NAME := OLIMEX_ESP32_GATEWAY-$(VERSION)
else ifdef CONFIG_BOARD_ESP32_DEMO
PROJECT_NAME := ESP32_DEMO-$(VERSION)
endif

PROJECT_VER = "$(VERSION)"

#EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common

include $(IDF_PATH)/make/project.mk

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'make flash'. 
SPIFFS_IMAGE_FLASH_IN_PROJECT := 1
$(eval $(call spiffs_create_partition_image,storage,spiffs))