/*
 * Created on Thu Jun 02 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "mdns.h"
#include "enchele.h"

static app_settings_t* app_settings;
static const char* TAG = "MDNS";
extern SemaphoreHandle_t settings_mutex;

void start_mdns_service(app_settings_t* settings)
{
  app_settings = settings;

xSemaphoreTake(settings_mutex, portMAX_DELAY);
  char * name = app_settings->thingname;
xSemaphoreGive(settings_mutex);
  ESP_ERROR_CHECK(mdns_init());
  mdns_hostname_set(name);
  mdns_instance_name_set("Enchele module");
  ESP_LOGI(TAG, "mDNS service started, URL: %s.local", name);
}