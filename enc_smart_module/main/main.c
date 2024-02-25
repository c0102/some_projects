/*
 * Created on Wed May 10 2022
 * by: Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele
 */

/*##################################################################
* TODO(uran): Initial support for the servers, (http, sockets, mdns)
*             -Progress: HTTP almost done, url endpoints can be added later, mDNS is done - very simple
*              sockets not added yet.
* TODO(uran): Initial website for the device
*             -Progress: The website has begun
* TODO(uran): Message parsing as complete as possible at this point
*             -Progress: A tentative initial system in place. Adding more message endpoints as we go! 
*
####################################################################*/

#include <stdio.h>
#include <string.h>
#include "connect.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "WiFi.h"
#include "parse.h"
#include "server.h"
#include "enc_mdns.h"
#include "led.h"
#include "enchele.h"
#include "relay.h"
#include "mqtt.h"
#include "hlw.h"
#include "driver/uart.h"
#include "btn.h"
#include "schedule.h"

static const char* TAG = "MAIN";


extern QueueHandle_t Message_queue;

void app_main(void){
   
   hlw_data_t hlw_data = {};
   static app_settings_t app_settings = ENCHELE_CONFIG_DEFAULT_SETTINGS();
   app_init(&app_settings);
   ESP_LOGI(TAG, "Thingname is %s, size of app_settings: %zu bytes", app_settings.thingname, sizeof(app_settings));

   ESP_ERROR_CHECK(nvs_flash_init());
   wifi_init();
   init_parser(1024*6, &app_settings);
   wifi_connect(&app_settings);
   init_server(&app_settings);
   start_mdns_service(&app_settings);
   init_led(8);
   init_relay(&app_settings);
   mqtt_init(&app_settings);
   hlw_init(&app_settings);
   init_btn(&app_settings);
   char time[64];
   get_current_time(time);
   while(1){
      //ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
      
      hlw_data = get_hlw_data();

      ESP_LOGI(TAG, "Voltage: %.02f V  Current: %.02f A  Active Pow: %.02f W  "
              "\nApparent Pow: %.02f VA  Active Energy: %f KWh  Apparent Energy: %f KWh \nPower Factor: %02f", 
               hlw_data.voltage, hlw_data.current, hlw_data.active_power, hlw_data.apparent_power,
               hlw_data.active_energy, hlw_data.apparent_energy, hlw_data.power_factor);
      
      vTaskDelay(pdMS_TO_TICKS(250));
   }
}
