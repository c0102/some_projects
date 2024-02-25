/*
 * Created on Sun Jun 12 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "enchele.h"

static const char* TAG = "ENCHELE";

static app_settings_t* app_settings;
SemaphoreHandle_t settings_mutex;


esp_err_t change_setting(app_settings_n setting, void* value ){
    ESP_LOGI(TAG, "change setting");
    if(xSemaphoreTake(settings_mutex, portMAX_DELAY))
    {
            switch (setting){
                case THINGNAME:{
                    char* thingname = (char*) value;
                    if(strnlen(thingname, CONFIG_MESSAGE_LENGTH) >= CONFIG_THINGNAME_LENGTH || strnlen(thingname, CONFIG_MESSAGE_LENGTH) <= 0 ){
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change thingname, given value is out of range |0 < len < 64 |");
                        return ESP_FAIL;
                    }
                    
                    memset(app_settings->thingname, 0, CONFIG_THINGNAME_LENGTH );
                    strncpy(app_settings->thingname, thingname, strnlen(thingname, CONFIG_THINGNAME_LENGTH));   
                    ESP_LOGI(TAG, "thingname successfully changed, new thingname: %s", app_settings->thingname);             
                    break; 
                }

                case WIFI_SSID:{
                    char* wifi_ssid = (char*) value;
                    ESP_LOGI(TAG, "new wifi ssid to be changed: %s", wifi_ssid);
                    if(strnlen(wifi_ssid, CONFIG_MESSAGE_LENGTH) >= CONFIG_WIFI_SSID_LENGTH || strnlen(wifi_ssid, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change wifi_ssid, given value is out of range |0 < len < 32 |");
                        return ESP_FAIL;
                    }
                    memset(app_settings->wifi_ssid, 0, CONFIG_WIFI_SSID_LENGTH );
                    strncpy(app_settings->wifi_ssid, wifi_ssid, strnlen(wifi_ssid, CONFIG_WIFI_SSID_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new wifi_ssid: %s", app_settings->wifi_ssid);             
                    
                    break;}
                case WIFI_PASS :{

                    char* wifi_pass = (char*) value;
                    if(strnlen(wifi_pass, CONFIG_MESSAGE_LENGTH) >= CONFIG_WIFI_PASS_LENGTH || strnlen(wifi_pass, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change wifi_pass, given value is out of range |0 < len < 64 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->wifi_pass, 0, CONFIG_WIFI_PASS_LENGTH );
                    strncpy(app_settings->wifi_pass, wifi_pass, strnlen(wifi_pass, CONFIG_WIFI_PASS_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new wifi_pass: %s", app_settings->wifi_pass);             
                    break;
                    }
                case AP_NAME:{
                    char* ap_name = (char*) value;
                    if(strnlen(ap_name, CONFIG_MESSAGE_LENGTH) >= CONFIG_AP_NAME_LENGTH || strnlen(ap_name, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change ap_name, given value is out of range |0 < len < 32 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->ap_name, 0, CONFIG_AP_NAME_LENGTH );
                    strncpy(app_settings->ap_name, ap_name, strnlen(ap_name, CONFIG_AP_NAME_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new ap_name: %s", app_settings->ap_name); 
                    break;
                    }
                case AP_PASS:{
                    char* ap_pass = (char*) value;
                    if(strnlen(ap_pass, CONFIG_MESSAGE_LENGTH) >= CONFIG_AP_PASS_LENGTH || strnlen(ap_pass, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change ap_pass, given value is out of range |0 < len < 64 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->ap_pass, 0, CONFIG_AP_PASS_LENGTH );
                    strncpy(app_settings->ap_pass, ap_pass, strnlen(ap_pass, CONFIG_AP_PASS_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new ap_pass: %s", app_settings->ap_pass);
                    break;
                    }
                case MQTT_HOST:{
                    char* mqtt_host = (char*) value;
                    if(strnlen(mqtt_host, CONFIG_MESSAGE_LENGTH) >= CONFIG_MQTT_HOST_URL_LENGTH || strnlen(mqtt_host, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change mqtt_host, given value is out of range |0 < len < 100 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->mqtt_host, 0, CONFIG_MQTT_HOST_URL_LENGTH );
                    strncpy(app_settings->mqtt_host, mqtt_host, strnlen(mqtt_host, CONFIG_MQTT_HOST_URL_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new mqtt_host: %s", app_settings->mqtt_host);
                    break;
                    }
                case MQTT_PORT:{
                    ESP_LOGI(TAG, "changeing mqtt port");
                    uint32_t mqtt_port = *(uint32_t*) value;
                    ESP_LOGI(TAG, "mqtt copied:  %d", mqtt_port);
                    if(mqtt_port >= 65535 || mqtt_port == 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change mqtt_port, given value is out of range |0 < len < 65535 |");
                        return ESP_FAIL;
                    }

                    app_settings->mqtt_port = mqtt_port;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new mqtt_port: %d", app_settings->mqtt_port);
                    break;
                    }
                case PUB_TOPIC:{
                    char* pub_topic = (char*) value;
                    if(strnlen(pub_topic, CONFIG_MESSAGE_LENGTH) >= CONFIG_MQTT_TOPIC_LENGTH || strnlen(pub_topic, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change pub_topic, given value is out of range |0 < len < 50 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->pub_topic, 0, CONFIG_MQTT_TOPIC_LENGTH );
                    strncpy(app_settings->pub_topic, pub_topic, strnlen(pub_topic, CONFIG_MQTT_TOPIC_LENGTH));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new pub_topic: %s", app_settings->pub_topic);
                    break;
                    }
                   
                case SUB_TOPIC:{
                    char* sub_topic = (char*) value;
                    if(strnlen(sub_topic, CONFIG_MESSAGE_LENGTH) >= CONFIG_MQTT_TOPIC_LENGTH || strnlen(sub_topic, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change sub_topic, given value is out of range |0 < len < 50 |");
                        return ESP_FAIL;
                    }

                    memset(app_settings->sub_topic, 0, CONFIG_MQTT_TOPIC_LENGTH );
                    strncpy(app_settings->sub_topic, sub_topic, strnlen(sub_topic, 50));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new sub_topic: %s", app_settings->sub_topic);
                    break;
                    }
                case INFO_TOPIC:{
                    char* info_topic = (char*) value;
                    if(strnlen(info_topic, CONFIG_MESSAGE_LENGTH) >= CONFIG_MQTT_TOPIC_LENGTH || strnlen(info_topic, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change info_topic, given value is out of range |0 < len < 50 |");
                        return ESP_FAIL;
                    }
                    
                    memset(app_settings->info_topic, 0, CONFIG_MQTT_TOPIC_LENGTH );
                    strncpy(app_settings->info_topic, info_topic, strnlen(info_topic, 50));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new info_topic: %s", app_settings->info_topic);
                    break;
                    }
                case LOG_TOPIC:{
                    char* log_topic = (char*) value;
                    if(strnlen(log_topic, CONFIG_MESSAGE_LENGTH) >= CONFIG_MQTT_TOPIC_LENGTH || strnlen(log_topic, CONFIG_MESSAGE_LENGTH) <= 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change log_topic, given value is out of range |0 < len < 50 |");
                        return ESP_FAIL;
                    }
                    
                    memset(app_settings->log_topic, 0, CONFIG_MQTT_TOPIC_LENGTH );
                    strncpy(app_settings->log_topic, log_topic, strnlen(log_topic, 50));   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new log_topic: %s", app_settings->log_topic);
                    break;
                    }
                case RELAY1:{
                   
                    uint8_t relay1 = *(uint8_t*) value;
                    if(relay1 >= 22)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change relay1, given value is out of range |0 < len < 22 |");
                        return ESP_FAIL;
                    }

                    app_settings->RELAY1 = relay1;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new relay1: %d", app_settings->RELAY1);
                    break;
                    }
#ifdef CONFIG_DUAL_MODULE
                case RELAY2:{
                    uint8_t relay2 = *(uint8_t*) value;
                    if(relay2 >= 22)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change relay2, given value is out of range |0 < len < 22 |");
                        return ESP_FAIL;
                    }

                    app_settings->RELAY2 = relay2;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new relay2: %d", app_settings->RELAY2);
                    break;
                    }
#endif
                case HLW:{
                    uint8_t hlw = *(uint8_t*) value;
                    if(hlw >= 22 )
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change hlw, given value is out of range |0 < len < 22 |");
                        return ESP_FAIL;
                    }

                    app_settings->HLW = hlw;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new hlw: %d", app_settings->HLW);
                    break;
                    }
                case V_COEFF:{
                    float v_coeff = *(float*) value;
                    if(v_coeff >= 5 || v_coeff < 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change v_coeff, given value is out of range |0 < len < 5 |");
                        return ESP_FAIL;
                    }

                    app_settings->v_coeff = v_coeff;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new v_coeff: %d", app_settings->v_coeff);
                    break;
                    }
                case C_SHUNT:{
                    float c_shunt = *(float*) value;
                    if(c_shunt >= 5 || c_shunt < 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change c_shunt, given value is out of range |0 < len < 5 |");
                        return ESP_FAIL;
                    }

                    app_settings->c_shunt = c_shunt;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new c_shunt: %d", app_settings->c_shunt);
                    break;
                }
                case C_COEFF:{
                    float c_coeff = *(float*) value;
                    if(c_coeff >= 5 || c_coeff < 0)
                    {
                        xSemaphoreGive(settings_mutex);
                        ESP_LOGE(TAG, "[CHANGE SETTINGS] Couldn't change c_coeff, given value is out of range |0 < len < 5 |");
                        return ESP_FAIL;
                    }

                    app_settings->c_coeff = c_coeff;   
                    ESP_LOGI(TAG, "wifi_ssid successfully changed, new c_coeff: %d", app_settings->c_coeff);
                    break;
                    }
                default:
                    break; 
            }
    }
    xSemaphoreGive(settings_mutex);
    return ESP_OK;

}


void get_current_time(char* time_string){
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);
    // Set timezone to central europe
    setenv("TZ", "CEST", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    strncpy(time_string, strftime_buf, 64);
    ESP_LOGI(TAG, "The time is: %s", time_string);
}

void app_init(app_settings_t* settings)
{
    app_settings = settings;
    settings_mutex = xSemaphoreCreateMutex();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}