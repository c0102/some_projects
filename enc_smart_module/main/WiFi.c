/*
 * Created on Wed May 10 2022
 * by: Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele
 */

#include "WiFi.h"
#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "enchele.h"


static const char* TAG = "wifi_connect";
app_settings_t* app_settings;
extern SemaphoreHandle_t settings_mutex;

void wifi_connect(app_settings_t* settings)
{
   app_settings = settings;

   xSemaphoreTake(settings_mutex, portMAX_DELAY);
   esp_err_t err = wifi_connect_sta(app_settings->wifi_ssid, app_settings->wifi_pass, 10000, 1);
   
   if(err){
      ESP_LOGE(TAG, "Failed to connect to sta %s. Starting AP", CONFIG_WIFI_SSID );
      wifi_connect_ap_sta(CONFIG_AP_NAME "_" CONFIG_THINGNAME , CONFIG_AP_PASS);     
   }
   xSemaphoreGive(settings_mutex);
}