/*
 * Created on Wed Nov 23 2022
 * by Uran Cabra, uran.cabra@enchele.com
 *
 */

#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "common.h"
#include "sht40_front.h"
#include "beacon.h"
#include "WiFi.h"
#include "dns_server.h"
//#include "temp_server.h"
#include "victron_uart.h"
#include "FO_http_server.h"
#include "fo_ethernet.h"
#include "btn.h"
#include "send_data_ws.h"
#include "day_night_logic.h"


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static const char* TAG = "MAIN";

// extern SemaphoreHandle_t settings_mutex;
// extern SemaphoreHandle_t sensor_values_mutex;
// extern SemaphoreHandle_t system_info_mutex;



//NOTE(uran): the order of ops is important but this is temporary
void app_main(void)
{
    
    const esp_partition_t *partition = esp_ota_get_running_partition();
    printf("Currently running partition: %s\r\n", partition->label);

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
	if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
		esp_ota_mark_app_valid_cancel_rollback();
	}
    }
    esp_app_desc_t* app_desc = esp_ota_get_app_description();
    printf("Currently running app version: %s\r\n", app_desc->version);

    static app_settings_t app_settings = FO_APP_CONFIG_DEFAULT_SETTINGS();
    static sensor_values_t sensor_values = {};
    static system_state_t sys_state = {};
    static outputs_state_t out_state = {};
    //sensor_values.yield_today = 120;
    sprintf(sys_state.ap_ip, "124.213.16.29" );
    sprintf(sys_state.app_version, app_desc->version);
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "\nHello FO! \n##############\n# Settings: \n# THINGNAME: %s\n# ch1-4 pins: %d, %d, %d, %d\n# "
            "i2c sda/scl pins : %d, %d\n# Solar Charge Controler RX pin: %d\n# AP_name and AP_pass: %s, %s\n# mqtt host: %s\n# mqtt port: %d\n##############\n",
            app_settings.thingname,app_settings.ch1_pin, app_settings.ch2_pin, app_settings.ch3_pin, app_settings.ch4_pin,
            app_settings.sda_sht_pin, app_settings.scl_sht_pin,app_settings.rx_solar_pin, app_settings.ap_name, app_settings.ap_pass,
            app_settings.mqtt_host, app_settings.mqtt_port);    

    app_init(&app_settings, &sys_state, &sensor_values, &out_state);

    wifi_connect(&app_settings);
    init_server(&app_settings);
    start_dns_server();
    init_eth(&app_settings, &sys_state);
    beacon_init(&app_settings, &out_state);
   
    sht40_init(&app_settings, &sensor_values);
    vbs_init(&app_settings, &sensor_values);
    init_btn(&app_settings);
    day_night_logic_init(&app_settings, &sys_state, &sensor_values, &out_state);

    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);
    
    ESP_LOGV(TAG, "APP init successful!");

    while(1){
        updateSysInfo();
xSemaphoreTake(sensor_values.mutex, portMAX_DELAY); xSemaphoreTake(sys_state.mutex, portMAX_DELAY);
        ESP_LOGI(TAG,"\rtemp: %.2f humid: %.2f  batt.voltage: %.2f "  
                     "batt.current: %.2f || %s || free memory: %dKb",
                sensor_values.temperature, sensor_values.humidity, 
                sensor_values.bat_voltage, sensor_values.bat_current,
                sys_state.day ? "day":"night", sys_state.free_memory);

xSemaphoreGive(sensor_values.mutex); xSemaphoreGive(sys_state.mutex);
        vTaskDelay(pdMS_TO_TICKS(10000));

    }
    
}
