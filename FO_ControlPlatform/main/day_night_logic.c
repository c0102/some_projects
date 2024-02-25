/*
 * Created on Wed Dec 14 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "common.h"
#include "beacon.h"
#include "FO_http_server.h"
#include "day_night_logic.h"


static const char* TAG = "DAY_NIGHT_LOGIC";
static app_settings_t* app_settings;
static system_state_t* sys_state;
static outputs_state_t* out_state;
static sensor_values_t* sens_values;



void set_blink_state(void* params){

    while(1){

        if(xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE &&
           xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
            
            if(sys_state->day){
                beacon_SLOW();
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    if(!out_state->ch1_latch && !out_state->ch1_state){
                        out_state->ch1_state = true;
                        change_switch_state(app_settings->ch1_pin, out_state->ch1_state);
                    }
                    if(!out_state->ch2_latch && !out_state->ch2_state){
                        out_state->ch2_state = true;
                        change_switch_state(app_settings->ch2_pin, out_state->ch2_state);
                    }
                    if(!out_state->ch3_latch && !out_state->ch3_state){
                        out_state->ch3_state = true;
                        change_switch_state(app_settings->ch3_pin, out_state->ch3_state);
                    }
                    if(!out_state->ch4_latch && !out_state->ch4_state){
                        out_state->ch4_state = true;
                        change_switch_state(app_settings->ch4_pin, out_state->ch4_state);
                    }
                    
                    xSemaphoreGive(out_state->mutex);
                }
                    sys_state->changed = false;
            }
            else{
                beacon_NORMAL();
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    if(out_state->ch1_state && ((esp_timer_get_time() - out_state->ch1_time_on) >= 300000000) && out_state->ch1_state){
                        out_state->ch1_state = false;
                        change_switch_state(app_settings->ch1_pin, out_state->ch1_state);
                        ESP_LOGI(TAG, " Turning ch1 off after 5min at night!");
                    }
                    if(out_state->ch2_state && ((esp_timer_get_time() - out_state->ch2_time_on) >= 300000000) && out_state->ch2_state){
                        out_state->ch2_state = false;
                        change_switch_state(app_settings->ch2_pin, out_state->ch2_state);
                        ESP_LOGI(TAG, " Turning ch2 off after 5min at night!");
                    }
                    if(out_state->ch3_state && ((esp_timer_get_time() - out_state->ch3_time_on) >= 300000000) && out_state->ch3_state){
                        out_state->ch3_state = false;
                        change_switch_state(app_settings->ch3_pin, out_state->ch3_state);
                        ESP_LOGI(TAG, " Turning ch3 off after 5min at night!");
                    }
                    if(out_state->ch4_state && ((esp_timer_get_time() - out_state->ch4_time_on) >= 300000000) && out_state->ch4_state){
                        out_state->ch4_state = false;
                        change_switch_state(app_settings->ch4_pin, out_state->ch4_state);
                        ESP_LOGI(TAG, " Turning ch4 off after 5min at night!");
                    }
                    xSemaphoreGive(out_state->mutex);
                }
            }
            xSemaphoreGive(sys_state->mutex);
            xSemaphoreGive(app_settings->mutex);
            
            
        }else{
            ESP_LOGE(TAG, "[set_blink_state] sys_state mutex take timeout!");
        }

        send_data_ws();//This function cannot be called inside taken mutexes
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void determine_day(void *params){

    while(1){
        
        if(xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(1000)) == pdTRUE){
            if(!sys_state->manual_override){
                if(xSemaphoreTake(sens_values->mutex, pdMS_TO_TICKS(1000)) == pdTRUE){

                    if(sens_values->panel_voltage >= 4.5 ){
                        sys_state->changed = sys_state->day ? false : true;
                        sys_state->day = true;
                        ESP_LOGD(TAG, "[determine_day] state set to day ");
                    }else{
                        sys_state->changed = sys_state->day ? true : false;
                        sys_state->day = false;
                        ESP_LOGD(TAG, "[determine_day] state set to night ");
                    }
                    xSemaphoreGive(sens_values->mutex);
                }else{
                    ESP_LOGE(TAG, "[determine_day] sens_values mutex take timeout!");
                }

            }

            xSemaphoreGive(sys_state->mutex);
        }else{
            ESP_LOGE(TAG, "[determine_day] sys_state mutex take timeout!");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t day_night_logic_init(app_settings_t* settings, system_state_t* system_state, sensor_values_t* sensor_values, outputs_state_t* outputs_state){

    app_settings = settings;
    sys_state = system_state;
    sens_values = sensor_values;
    out_state = outputs_state;
    xTaskCreatePinnedToCore(determine_day, "determine_day", 1024*3, NULL, 9, NULL, 1);
    xTaskCreatePinnedToCore(set_blink_state, "set_blink_state", 1024*5, NULL, 9, NULL, 1);
    return ESP_OK;
}