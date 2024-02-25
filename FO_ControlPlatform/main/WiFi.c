/*
 * Created on Fri Nov 25 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */

#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "common.h"
#include "WiFi.h"


static const char* TAG = "wifi_connect";
static app_settings_t* app_settings;
// extern SemaphoreHandle_t settings_mutex;

void wifi_connect(app_settings_t* settings)
{
   app_settings = settings;
   wifi_init();

   xSemaphoreTake(app_settings->mutex, portMAX_DELAY);
      wifi_connect_ap(app_settings->ap_name, app_settings->ap_pass);
      ESP_LOGI(TAG, "WiFI AP with name: %s and pass: %s started", app_settings->ap_name, app_settings->ap_pass);
   xSemaphoreGive(app_settings->mutex);
}