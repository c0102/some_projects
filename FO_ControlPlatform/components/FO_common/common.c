/*
 * Created on Thu Nov 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */

#include <sys/param.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "string.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "common.h"
#include "FO_http_server.h"

static const char* TAG = "APP_COMMON";
static app_settings_t* app_settings;
static system_state_t* sys_state;
static sensor_values_t* sens_values;
static outputs_state_t* out_state;


esp_err_t init_switches(){

    if(xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(150)) == pdTRUE){
        
        gpio_pad_select_gpio(app_settings->ch1_pin);
        gpio_set_direction(app_settings->ch1_pin, GPIO_MODE_OUTPUT);
        gpio_set_intr_type(app_settings->ch1_pin, GPIO_INTR_DISABLE);
        
        gpio_pad_select_gpio(app_settings->ch2_pin);
        gpio_set_direction(app_settings->ch2_pin, GPIO_MODE_OUTPUT);
        gpio_set_intr_type(app_settings->ch2_pin, GPIO_INTR_DISABLE);
        
        gpio_pad_select_gpio(app_settings->ch3_pin);
        gpio_set_direction(app_settings->ch3_pin, GPIO_MODE_OUTPUT);
        gpio_set_intr_type(app_settings->ch3_pin, GPIO_INTR_DISABLE);
        
        gpio_pad_select_gpio(app_settings->ch4_pin);
        gpio_set_direction(app_settings->ch4_pin, GPIO_MODE_OUTPUT);
        gpio_set_intr_type(app_settings->ch4_pin, GPIO_INTR_DISABLE);
        
        if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(100)) == pdTRUE){
            gpio_set_level(app_settings->ch1_pin, out_state->ch1_state);
            gpio_set_level(app_settings->ch2_pin, out_state->ch2_state);
            gpio_set_level(app_settings->ch3_pin, out_state->ch3_state);
            gpio_set_level(app_settings->ch4_pin, out_state->ch4_state);

        }else{
            ESP_LOGE(TAG,"Taking mutex of output_states failed!");
            xSemaphoreGive(app_settings->mutex);
            return ESP_FAIL;   
        }

        xSemaphoreGive(out_state->mutex);

    }else{
        ESP_LOGE(TAG,"Taking mutex of app_settigns failed!");
        return ESP_FAIL;
    }
    xSemaphoreGive(app_settings->mutex);
    return ESP_OK;
}

// static QueueHandle_t message_queue;

// void message_parse(void* param){

//     message_t* message;
//     while(1){
//         if(xQueueReceive(message_queue, &(message), portMAX_DELAY)){}
//     }

// }

int check_if_valid_pin_and_return(cJSON* settings_json, char* pin, int current_value)
{
    ESP_LOGI(TAG, "[check if valid] pin: %s, current value: %d", pin, current_value);
    int pin_num = (int)strtol(cJSON_GetObjectItem(settings_json, pin)->valuestring, (char **)NULL, 10);
    return GPIO_IS_VALID_GPIO(pin_num) ? pin_num : current_value;
}


esp_err_t change_settings(char* message, int len){
    cJSON* root = cJSON_Parse(message);
    esp_err_t res = ESP_OK;
    if(cJSON_GetObjectItem(root, "cmd")){
        if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "change_settings") == 0){
            if(cJSON_GetObjectItem(root, "settings")){
                cJSON* settings_json = cJSON_GetObjectItem(root, "settings");
                if(xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE){

                    /**
                     * Check if the value exists in the settings json then convert the string value to int 
                     * if the value doesn't exist in the settings json don't change the old value
                     * 
                     */
                    app_settings->ch1_pin = cJSON_GetObjectItem(settings_json, "ch1_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "ch1_pin", app_settings->ch1_pin) :
                                            app_settings->ch1_pin;
                    app_settings->ch2_pin = cJSON_GetObjectItem(settings_json, "ch2_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "ch2_pin", app_settings->ch2_pin) : 
                                            app_settings->ch2_pin;
                    app_settings->ch3_pin = cJSON_GetObjectItem(settings_json, "ch3_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "ch3_pin", app_settings->ch3_pin) : 
                                            app_settings->ch3_pin;
                    app_settings->ch4_pin = cJSON_GetObjectItem(settings_json, "ch4_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "ch4_pin", app_settings->ch4_pin) : 
                                            app_settings->ch4_pin;
                    app_settings->sda_sht_pin = cJSON_GetObjectItem(settings_json, "sda_sht_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "sda_sht_pin", app_settings->sda_sht_pin): 
                                            app_settings->sda_sht_pin;
                    app_settings->scl_sht_pin = cJSON_GetObjectItem(settings_json, "scl_sht_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "scl_sht_pin", app_settings->scl_sht_pin): 
                                            app_settings->scl_sht_pin;
                    app_settings->rx_solar_pin = cJSON_GetObjectItem(settings_json, "rx_solar_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "rx_solar_pin", app_settings->rx_solar_pin): 
                                            app_settings->rx_solar_pin;
                    app_settings->beacon_pin = cJSON_GetObjectItem(settings_json, "beacon_pin") ? 
                                            check_if_valid_pin_and_return(settings_json, "beacon_pin", app_settings->beacon_pin) : 
                                            app_settings->beacon_pin;

                    app_settings->mqtt_port = cJSON_GetObjectItem(settings_json, "mqtt_port") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "mqtt_port")->valuestring, (char **)NULL, 10) : 
                                            app_settings->mqtt_port;
                                            
                    app_settings->slow_blink_intensity = cJSON_GetObjectItem(settings_json, "slow_blink_intensity") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "slow_blink_intensity")->valuestring, (char **)NULL, 10):
                                            app_settings->slow_blink_intensity;
                    app_settings->slow_blink_on_duration = cJSON_GetObjectItem(settings_json, "slow_blink_on_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "slow_blink_on_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->slow_blink_on_duration;                                            
                    app_settings->slow_blink_off_duration = cJSON_GetObjectItem(settings_json, "slow_blink_off_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "slow_blink_off_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->slow_blink_off_duration;
                    app_settings->normal_blink_intensity = cJSON_GetObjectItem(settings_json, "normal_blink_intensity") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "normal_blink_intensity")->valuestring, (char **)NULL, 10):
                                            app_settings->normal_blink_intensity;
                    app_settings->normal_blink_on_duration =  cJSON_GetObjectItem(settings_json, "normal_blink_on_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "normal_blink_on_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->normal_blink_on_duration;                
                    app_settings->normal_blink_off_duration = cJSON_GetObjectItem(settings_json, "normal_blink_off_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "normal_blink_off_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->normal_blink_off_duration;                
                    app_settings->fast_blink_intensity = cJSON_GetObjectItem(settings_json, "fast_blink_intensity") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "fast_blink_intensity")->valuestring, (char **)NULL, 10):
                                            app_settings->fast_blink_intensity;
                    app_settings->fast_blink_on_duration = cJSON_GetObjectItem(settings_json, "fast_blink_on_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "fast_blink_on_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->fast_blink_on_duration;
                    app_settings->fast_blink_off_duration = cJSON_GetObjectItem(settings_json, "fast_blink_off_duration") ? 
                                            (int)strtol(cJSON_GetObjectItem(settings_json, "fast_blink_off_duration")->valuestring, (char **)NULL, 10):
                                            app_settings->fast_blink_off_duration;                 

                    /** Strings are checked and stored from here */
                    if(cJSON_GetObjectItem(settings_json, "thingname")){
                        memset(app_settings->thingname, 0, 64);
                        strcpy(app_settings->thingname, cJSON_GetObjectItem(settings_json, "thingname")->valuestring);
                    }

                    if(cJSON_GetObjectItem(settings_json, "ap_name")){
                        if(strnlen(cJSON_GetObjectItem(settings_json, "ap_name")->valuestring, CONFIG_AP_NAME_LENGTH) >= 1){
                            memset(app_settings->ap_name, 0, CONFIG_AP_NAME_LENGTH);
                            strcpy(app_settings->ap_name, cJSON_GetObjectItem(settings_json, "ap_name")->valuestring);
                        }
                    }

                    if(cJSON_GetObjectItem(settings_json, "ap_pass")){
                        
                        /** The ap pass length shouldn't be smaller than 8 but it can also be 0 if you need to remove the password */

                        if(strnlen(cJSON_GetObjectItem(settings_json, "ap_pass")->valuestring,CONFIG_AP_PASS_LENGTH ) >= 8 ||
                           strnlen(cJSON_GetObjectItem(settings_json, "ap_pass")->valuestring,CONFIG_AP_PASS_LENGTH ) == 0){   
                            memset(app_settings->ap_pass, 0, CONFIG_AP_PASS_LENGTH);
                            strcpy(app_settings->ap_pass, cJSON_GetObjectItem(settings_json, "ap_pass")->valuestring);
                        }else{
                            ESP_LOGE(TAG,"[change settings] The given AP pass length is smaller than 8, setting not set!");
                        }
                    }
                    
                    if(cJSON_GetObjectItem(settings_json, "mqtt_host")){
                        memset(app_settings->mqtt_host, 0, CONFIG_MQTT_HOST_URL_LENGTH);
                        strcpy(app_settings->mqtt_host, cJSON_GetObjectItem(settings_json, "mqtt_host")->valuestring);
                    }
                    
                    xSemaphoreGive(app_settings->mutex);
                    res = save_settings();
                }else{}
            }else{}
        }else if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "reset_settings") == 0){
            res = delete_settings_file();
        }else{}
    }else{}
                                   
    cJSON_Delete(root);

    if(res == ESP_OK) esp_restart();

    memset(message, 0, len);
    sprintf(message, "{\"error\":\"Something went wrong when executing this command\"}");
    return res;
}


esp_err_t save_settings(){

    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/data",
      .partition_label = "data",
      .max_files = 5,
      .format_if_mount_failed = false};

    esp_err_t res = esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);
    ESP_ERROR_CHECK_WITHOUT_ABORT(res);

    if(res == ESP_OK){
        FILE* file = fopen("/data/settings.dat", "w");
        if(file == NULL){
            ESP_LOGE(TAG, "[save settings] couldn't open settings file ");
            return ESP_FAIL;
        }
        if(xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
            if(!fwrite(app_settings, sizeof(app_settings_t), 1, file)){
                ESP_LOGE(TAG, "[save settings] file write error");
                return ESP_FAIL;
                esp_vfs_spiffs_unregister("data");
                return ESP_FAIL;
            }
            xSemaphoreGive(app_settings->mutex);
        }else{
            ESP_LOGE(TAG,"[save settings] app settings mutex take timeout");
            fclose(file);

        }
        
        fclose(file);      
    }else{
        ESP_LOGE(TAG, "[save settings] spiffs register unsuccesful!");
        return res;
    }

    ESP_LOGI(TAG, "[save settings] settings saved sucessfully!");
    esp_vfs_spiffs_unregister("data");
    return ESP_OK;
}

esp_err_t delete_settings_file(){

    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/data",
      .partition_label = "data",
      .max_files = 5,
      .format_if_mount_failed = false};

    esp_err_t res = esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);
    ESP_ERROR_CHECK_WITHOUT_ABORT(res);
    if(res == ESP_OK){
        int ret = remove("/data/settings.dat");
        if(!ret){
            res = ESP_FAIL;
        }
    }

    esp_vfs_spiffs_unregister("data");
    return res;

}



void check_settings_and_update(){

    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/data",
      .partition_label = "data",
      .max_files = 5,
      .format_if_mount_failed = true};

    esp_err_t res = esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);
    ESP_ERROR_CHECK_WITHOUT_ABORT(res);

    if(res == ESP_OK){
        FILE* file = fopen("/data/settings.dat", "rb");
        if(file == NULL){
            ESP_LOGI(TAG, "[check settings] settings file not found, proceeding with factory settings");
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_spiffs_unregister("data"));
            return;
        }    
        SemaphoreHandle_t temp = app_settings->mutex;
        if(xSemaphoreTake(temp, pdMS_TO_TICKS(500)) == pdTRUE){
            fread(app_settings, sizeof(app_settings_t), 1, file);
            app_settings->mutex = temp;

            ESP_LOGI(TAG, "\nSettings read from file \n##############\n# Settings: \n# THINGNAME: %s\n# ch1-4 pins: %d, %d, %d, %d\n# "
                "i2c sda/scl pins : %d, %d\n# Solar Charge Controler RX pin: %d\n# AP_name and AP_pass: %s, %s\n# mqtt host: %s\n# mqtt port: %d"
                "\nBeacon settings:\nSlow Blink:\n\tIntensity:%d\n\tOn Duration: %d\n\tOff Duration: %d\nNormal Blink:"
                "\n\tIntensity: %d\n\tOn Duration: %d\n\tOff Duration: %d\nFast Blink:\n\tIntensity: %d"
                "\n\tOn Duration: %d\n\tOff Duration:  %d\n##############\n",
                app_settings->thingname,app_settings->ch1_pin, app_settings->ch2_pin, app_settings->ch3_pin, app_settings->ch4_pin,
                app_settings->sda_sht_pin, app_settings->scl_sht_pin,app_settings->rx_solar_pin, app_settings->ap_name, app_settings->ap_pass,
                app_settings->mqtt_host, app_settings->mqtt_port, app_settings->slow_blink_intensity,app_settings->slow_blink_on_duration, app_settings->slow_blink_off_duration,
                app_settings->normal_blink_intensity,app_settings->normal_blink_on_duration, app_settings->normal_blink_off_duration,app_settings->fast_blink_intensity,
                app_settings->fast_blink_on_duration, app_settings->fast_blink_off_duration ); 

            xSemaphoreGive(temp);
        }else{
            ESP_LOGE(TAG, "[check settings] app settings mutex take timeout!");
        }

        fclose(file);
        esp_vfs_spiffs_unregister("data");

    }else{
        ESP_LOGE(TAG, "[check settings] spiffs register error!");
    }
}

void app_init(app_settings_t* settings, system_state_t* system_info, sensor_values_t* sensor_values, outputs_state_t* out_st){

    app_settings = settings;
    sys_state = system_info;
    sens_values = sensor_values;
    out_state = out_st;

    app_settings->mutex = xSemaphoreCreateMutex();
    sens_values->mutex = xSemaphoreCreateMutex();
    sys_state->mutex = xSemaphoreCreateMutex();
    out_state->mutex = xSemaphoreCreateMutex();

    init_switches();

    check_settings_and_update();

    // message_queue = xQueueCreate( 10, sizeof(message_t *));
    // xTaskCreatePinnedToCore(message_parse, "message_parse", 1024*8, NULL,10, NULL, 1 );


}



esp_err_t change_switch_state(uint8_t pin, uint8_t state){
   esp_err_t res = gpio_set_level(pin, state);
   //send_data_ws();
   return res;
}

/** TODO(uran): refactor this function */
void parse_switch_message(char* message){
    cJSON *root = cJSON_Parse(message);
    if(cJSON_GetObjectItem(root, "cmd")){
        ESP_LOGD(TAG,"Parsing message");
        if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_ch1") == 0){
            if(cJSON_GetObjectItem(root, "is_on")){
                ESP_LOGD(TAG,"Checking is_on");
                cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                bool is_on = cJSON_IsTrue(is_on_json);
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE && 
                   xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE&&
                   xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    
                    out_state->ch1_state = is_on;
                    if(is_on){ 
                        out_state->ch1_time_on = esp_timer_get_time();
                        out_state->ch1_latch = false;
                    }else{
                        if(sys_state->day) out_state->ch1_latch = true;
                    }
                    ESP_ERROR_CHECK(change_switch_state(app_settings->ch1_pin,out_state->ch1_state ));
                    ESP_LOGD(TAG, "CH1 toggled %s", is_on ? "on":"off");
                    xSemaphoreGive(out_state->mutex);
                    xSemaphoreGive(app_settings->mutex);
                    xSemaphoreGive(sys_state->mutex);
                }else{
                    ESP_LOGE(TAG, "Either the app_settings mutex or the outputs_states mutex couldn't be taken! CH1 state didn't change!");
                }
                

            }
        }else if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_ch2") == 0){
            if(cJSON_GetObjectItem(root, "is_on")){
                ESP_LOGI(TAG,"Checking is_on");
                cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                bool is_on = cJSON_IsTrue(is_on_json);
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE && 
                   xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE &&
                   xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    
                    out_state->ch2_state = is_on;
                    if(is_on){ 
                        out_state->ch2_time_on = esp_timer_get_time();
                        out_state->ch2_latch = false;
                    }else{
                        if(sys_state->day) out_state->ch2_latch = true;
                    }
                    
                    ESP_ERROR_CHECK(change_switch_state(app_settings->ch2_pin, out_state->ch2_state ));
                    ESP_LOGI(TAG, "CH2 toggled %s", is_on ? "on":"off");
                    
                    
                    xSemaphoreGive(out_state->mutex);
                    xSemaphoreGive(app_settings->mutex);
                    xSemaphoreGive(sys_state->mutex);
                }else{
                    ESP_LOGE(TAG, "Either the app_settings mutex or the outputs_states mutex couldn't be taken! CH2 state didn't change!");
                }
                

            }
        }else if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_ch3") == 0){
            if(cJSON_GetObjectItem(root, "is_on")){
                ESP_LOGI(TAG,"Checking is_on");
                cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                bool is_on = cJSON_IsTrue(is_on_json);
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(100)) == pdTRUE && 
                   xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(100)) == pdTRUE&&
                   xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    
                    out_state->ch3_state = is_on;
                    if(is_on){ 
                        out_state->ch3_time_on = esp_timer_get_time();
                        out_state->ch3_latch = false;
                    }else{
                        if(sys_state->day) out_state->ch3_latch = true;
                    }
                    ESP_ERROR_CHECK(change_switch_state(app_settings->ch3_pin, out_state->ch3_state ));
                    ESP_LOGI(TAG, "CH3 toggled %s", is_on ? "on":"off");
                    xSemaphoreGive(out_state->mutex);
                    xSemaphoreGive(app_settings->mutex);
                    xSemaphoreGive(sys_state->mutex);
                }else{
                    ESP_LOGE(TAG, "Either the app_settings mutex or the outputs_states mutex couldn't be taken! CH3 state didn't change!");
                }
                

            }
        }else if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_ch4") == 0){
            if(cJSON_GetObjectItem(root, "is_on")){
                ESP_LOGI(TAG,"Checking is_on");
                cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                bool is_on = cJSON_IsTrue(is_on_json);
                if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(100)) == pdTRUE && 
                   xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(100)) == pdTRUE&&
                   xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE){
                    
                    out_state->ch4_state = is_on;
                    if(is_on){ 
                        out_state->ch4_time_on = esp_timer_get_time();
                        out_state->ch4_latch = false;
                    }else{
                        if(sys_state->day) out_state->ch4_latch = true;
                    }
                    ESP_ERROR_CHECK(change_switch_state(app_settings->ch4_pin, out_state->ch4_state ));
                    ESP_LOGI(TAG, "CH4 toggled %s", is_on ? "on":"off");

                    xSemaphoreGive(out_state->mutex);
                    xSemaphoreGive(app_settings->mutex);
                    xSemaphoreGive(sys_state->mutex);
                    
                }else{
                    ESP_LOGE(TAG, "Either the app_settings mutex or the outputs_states mutex couldn't be taken! CH4 state didn't change!");
                }
                

            }
        }
    }
    cJSON_Delete(root);
    
}


void parse_manual_control_message(char* message){
    bool manual_override = false;
    bool day = false;
    cJSON* root = cJSON_Parse(message);
    if(cJSON_GetObjectItem(root, "cmd")){
        if(strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "manual_control") == 0){
            if(cJSON_GetObjectItem(root, "manual_override")){
                cJSON* manual_json = cJSON_GetObjectItem(root, "manual_override");
                manual_override = cJSON_IsTrue(manual_json);
            }else{
                cJSON_Delete(root);
                return;
            }
            if(cJSON_GetObjectItem(root, "day")){
                cJSON* day_json = cJSON_GetObjectItem(root, "day");
                day = cJSON_IsTrue(day_json);
            }else{
                cJSON_Delete(root);
                return;
            }
            if(xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(500))== pdTRUE){

                sys_state->manual_override = manual_override;
                if(manual_override){
                    if(day != sys_state->day){ 
                        sys_state->day = day;
                        sys_state->changed = true;
                    }
                }
                xSemaphoreGive(sys_state->mutex);
            }
        }
    }
    cJSON_Delete(root);
}

void parse_state_message(char* message, int len){

    memset(message, 0, len);
    if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(150)) == pdTRUE){
        snprintf(message, len, "{\"ch1\":%s, \"ch2\":%s, \"ch3\":%s, \"ch4\":%s," 
                               "\"ch1_latch\":%s, \"ch2_latch\":%s,\"ch3_latch\":%s,\"ch4_latch\":%s }",
                 out_state->ch1_state ? "true":"false", out_state->ch2_state ? "true":"false", 
                 out_state->ch3_state ? "true":"false", out_state->ch4_state ? "true":"false",
                 out_state->ch1_latch ? "true":"false", out_state->ch2_latch ? "true":"false", 
                 out_state->ch3_latch ? "true":"false", out_state->ch4_latch ? "true":"false" );
     xSemaphoreGive(out_state->mutex);   
    }
}

void parse_data_message(char* message,int len){

    memset(message, 0, len);
    if(xSemaphoreTake(sens_values->mutex, pdMS_TO_TICKS(150)) == pdTRUE){

        snprintf(message, len, "{\"temperature\":%.2f, \"battery-voltage\":%.2f, \"solar-power\":%.2f," 
                 "\"yield-today\":%.4f, \"yield-yesterday\":%.4f, \"load-current\":%.2f," 
                 "\"battery-current\":%.2f, \"humidity\":%.2f}",
                 sens_values->temperature, sens_values->bat_voltage, sens_values->panel_pwr,
                 sens_values->yield_today,  sens_values->yield_yesterday,  sens_values->load_current,  
                 sens_values->bat_current,  sens_values->humidity);

        xSemaphoreGive(sens_values->mutex);   
    }

    ESP_LOGD(TAG, "[parse data message] message:\n%s", message);
}

void parse_settings_message(char* message,int len){

    memset(message, 0, len);
    if(xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(250)) == pdTRUE){

        snprintf(message, len, "{\"thingname\":\"%s\", \"ap_name\":\"%s\", \"ap_pass\":\"%s\", \"slow_blink_intensity\":\"%d\","
                "\"slow_blink_on_duration\":\"%d\",\"slow_blink_off_duration\":\"%d\", \"normal_blink_intensity\":\"%d\","
                "\"normal_blink_on_duration\":\"%d\", \"normal_blink_off_duration\":\"%d\", \"fast_blink_intensity\":\"%d\","
                "\"fast_blink_on_duration\":\"%d\", \"fast_blink_off_duration\":\"%d\"}",
                app_settings->thingname, app_settings->ap_name, app_settings->ap_pass, app_settings->slow_blink_intensity,
                app_settings->slow_blink_on_duration, app_settings->slow_blink_off_duration, app_settings->normal_blink_intensity,
                app_settings->normal_blink_on_duration, app_settings->normal_blink_off_duration, app_settings->fast_blink_intensity,
                app_settings->fast_blink_on_duration, app_settings->fast_blink_off_duration);
        xSemaphoreGive(app_settings->mutex);   
    }else{
        ESP_LOGE(TAG,"[parse_settings], app_settings mutex take timeout!");
    }

    ESP_LOGD(TAG, "[parse data message] message:\n%s", message);
}

void parse_in_state_message(char* message,int len){
    
    memset(message, 0, len);
    if(xSemaphoreTake(sys_state->mutex, pdMS_TO_TICKS(150)) == pdTRUE){
        
        snprintf(message, len, "{\"free_memory\":%d, \"min_free_memory\":%d,"
                 "\"day\":\"%s\", \"changed\":%s, \"manual_override\":%s,"
                 "\"ap_ip\":\"%s\", \"eth_ip\":\"%s\", \"app_version\":\"%s\"}",sys_state->free_memory, sys_state->min_free_memory,
                  sys_state->day? "day":"night", sys_state->changed? "true":"false", 
                  sys_state->manual_override ? "true":"false", sys_state->ap_ip,
                  sys_state->eth_ip, sys_state->app_version);

        ESP_LOGD(TAG,"in_state json: %s ", message);
        xSemaphoreGive(sys_state->mutex);
    }
}



//TODO(uran): Properly manage this function. 
//            It shouldn't delay indef. when taking the mutex and should return an err in case of not updating the stuct
esp_err_t updateSysInfo()
{
xSemaphoreTake(sys_state->mutex, portMAX_DELAY);
    sys_state->free_memory = esp_get_free_heap_size() / 1024;
    sys_state->min_free_memory = esp_get_minimum_free_heap_size() /1024;
xSemaphoreGive(sys_state->mutex);
    return ESP_OK;
}



// esp_err_t recieve_message(message_t* message){


//     if(message == NULL || message->message == NULL || message->response == NULL){
//         ESP_LOGE(TAG,"message and/or response char* shouldn't point to NULL");
//         return ESP_FAIL;
//     }
    
    

//     if(xQueueSend(message_queue, message, portMAX_DELAY) == pdTRUE){}
    
//     //TODO(uran): handle this better
//     if(xSemaphoreTake(message->response_mutex, pdMS_TO_TICKS(500)) != pdTRUE){
//         ESP_LOGE(TAG,"response timeout!");
//         strcpy(message->response, "No response");
//         return ESP_OK;
//     }
//     return ESP_OK;
// }