/*
 * Created on Wed Dec 14 2022
 * by Uran Cabra, uran.cabra@enchele.com
 *
 * NOTE: functions for periodically sending data to the website through the websocket connection
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "esp_log.h"
#include "common.h"
#include "FO_http_server.h"

const char* TAG = "SEND_DATA";
static app_settings_t* app_settings;



void periodically_send_data(void* params){

    while(1){
        send_data_ws();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

esp_err_t init_send_data_ws(app_settings_t* settings){

    app_settings = settings;

    xTaskCreatePinnedToCore(periodically_send_data, "periodically_send_data", 1024*5, NULL, 9, NULL, 1);
    return ESP_OK;

}