/*
 * Created on Tue May 10 2022
 * by: Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele
 */


//TODO(uran): implement response passing using either Mutexes or Semaphores
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "cJSON.h"
#include "parse.h"
#include "esp_wifi.h"
#include "led.h"
#include "relay.h"
#include "enchele.h"




#define MAX_APs 10
static const char* TAG = "PARSE";
static app_settings_t* app_settings;
extern SemaphoreHandle_t settings_mutex;

QueueHandle_t Message_queue = NULL;

static char* get_aps()
{
  esp_wifi_set_mode(WIFI_MODE_APSTA);

  wifi_scan_config_t wifi_scan_config = {
      .bssid = 0,
      .ssid = 0,
      .channel = 0,
      .show_hidden = true};

  esp_wifi_scan_start(&wifi_scan_config, true);

  wifi_ap_record_t wifi_ap_record[MAX_APs];
  uint16_t ap_count = MAX_APs;
  esp_wifi_scan_get_ap_records(&ap_count, wifi_ap_record);
  cJSON *wifi_scan_json = cJSON_CreateArray();
  for (size_t i = 0; i < ap_count; i++)
  {
    cJSON *element = cJSON_CreateObject();
    ESP_LOGI(TAG, "In AP scan, ssid from the scan before JSON: %s", (char *)wifi_ap_record[i].ssid);
    cJSON_AddStringToObject(element, "ssid", (char *)wifi_ap_record[i].ssid);
    cJSON_AddNumberToObject(element, "rssi", wifi_ap_record[i].rssi);
    cJSON_AddItemToArray(wifi_scan_json, element);
  }
  char *json_str = cJSON_PrintUnformatted(wifi_scan_json);
  cJSON_Delete(wifi_scan_json);
  return json_str;
}


static char* get_settings()
{
    xSemaphoreTake(settings_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "settings_mutex taken");
    char time[64];
    get_current_time(time);
    cJSON *data = cJSON_CreateObject();
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "date-time", time);
    cJSON_AddStringToObject(data, "Thingname", app_settings->thingname);
    cJSON_AddStringToObject(data, "WiFi SSID", app_settings->wifi_ssid);
    cJSON_AddStringToObject(data, "WiFi PASS", app_settings->wifi_pass);
    cJSON_AddStringToObject(data, "AP name", app_settings->ap_name);
    cJSON_AddStringToObject(data, "AP pass", app_settings->ap_pass);
    cJSON_AddStringToObject(data, "mqtt host", app_settings->mqtt_host);
    cJSON_AddNumberToObject(data, "mqtt port", app_settings->mqtt_port);
    cJSON_AddStringToObject(data, "pub topic", app_settings->pub_topic);
    cJSON_AddStringToObject(data, "sub topic", app_settings->sub_topic);
    cJSON_AddStringToObject(data, "info topic", app_settings->info_topic);
    cJSON_AddStringToObject(data, "log topic", app_settings->log_topic);
    cJSON_AddNumberToObject(data, "Relay1", app_settings->RELAY1);
    cJSON_AddNumberToObject(data, "hlw pin", app_settings->HLW);
    cJSON_AddNumberToObject(data, "v_coeff", app_settings->v_coeff);
    cJSON_AddNumberToObject(data, "c_shunt", app_settings->c_shunt);
    cJSON_AddNumberToObject(data, "c_coeff", app_settings->c_coeff);

    xSemaphoreGive(settings_mutex);
    ESP_LOGI(TAG, "settings_mutex given");
    cJSON_AddItemToObject(resp, "settings", data);

    char* buff = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    return buff;
}


static void get_state(char* buff)
{
    memset(buff,0,strnlen(buff, CONFIG_MESSAGE_LENGTH));
    bool state = get_relay_state();
    char time[64];
    get_current_time(time);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "date-time", time);
    cJSON_AddBoolToObject(root, "is_on", state);
 xSemaphoreTake(settings_mutex, portMAX_DELAY);
    cJSON_AddStringToObject(root, "id", app_settings->thingname);
 xSemaphoreGive(settings_mutex);
    char * t = cJSON_PrintUnformatted(root);
    strncpy(buff, t, strnlen(t,CONFIG_MESSAGE_LENGTH));
    cJSON_Delete(root);
    cJSON_free(t);
}

void message_parse(void* params)
{
    message_t recv_message = {0};
    message_t resp_message = {0};

    cJSON *root;
    
    while(1){
        if(xQueueReceive(Message_queue, (void *)&recv_message, portMAX_DELAY )){
            ESP_LOGI(TAG, "Recieved message: %s \n message address: %p", recv_message.body, &recv_message);
            root = cJSON_Parse(recv_message.body);

            

            if (cJSON_GetObjectItem(root, "id")){
                xSemaphoreTake(settings_mutex, portMAX_DELAY);
                ESP_LOGI(TAG, "settings_mutex taken");
                if (strcmp(cJSON_GetObjectItem(root, "id")->valuestring, app_settings->thingname) == 0){
                    xSemaphoreGive(settings_mutex);
                    ESP_LOGI(TAG, "settings_mutex given");
                    if (cJSON_GetObjectItem(root, "cmd")){

                        if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "get_aps") == 0){

                            char *aps = get_aps(); // HAS TO be deleted, use cJSON_free()
                            strcpy(resp_message.body, aps);
                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                ESP_LOGI(TAG, "Queue response sent!");
                            }
                            cJSON_free(aps);
                        }
                        else if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_led") == 0){

                            if (cJSON_GetObjectItem(root, "is_on")){

                                cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                                bool is_on = cJSON_IsTrue(is_on_json);
                                toggle_led_red(is_on);
                                ESP_LOGI(TAG, "Turning the led %s", is_on ? "on" : "off");
                                // cJSON_Delete(is_on_json);
                                char state[100]; 
                                get_state(state);
                                strcpy(resp_message.body, state);
                                if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, pdMS_TO_TICKS(500))){

                                    ESP_LOGI(TAG, "Queue response sent!");
                                }else
                                {
                                    ESP_LOGI(TAG, "Queue response not sent!");
                                }
                            }
                            else{

                                ESP_LOGI(TAG, "No toggle command for the Led!");
                            }
                        }
                        else if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "toggle_relay") == 0){
                            // ESP_LOGI(TAG, "toggle relay");
                            cJSON *is_on_json = cJSON_GetObjectItem(root, "is_on");
                            bool is_on = cJSON_IsTrue(is_on_json);
                            // ESP_LOGI(TAG,"is_on:%s" is_on ? "true":"false");
                            toggle_relay(is_on);
                            ESP_LOGI(TAG, "Turning the relay %s", is_on ? "on" : "off");
                            // cJSON_Delete(is_on_json);
                            char state[100];
                            get_state(state);
                            strcpy(resp_message.body, state);
                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                ESP_LOGI(TAG, "Queue response sent!");
                            }
                        }
                        else if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "get_settings") == 0){

                            char *buff = get_settings();
                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                            cJSON_free(buff);
                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY))
                            {
                                ESP_LOGI(TAG, "Queue response sent!");
                            }
                        }
                        else if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "change_setting") == 0){

                            xSemaphoreGive(settings_mutex); // Give the mutex because we're about to call cahnge_setting()
                            ESP_LOGI(TAG, "settings_mutex given");
                            if (cJSON_GetObjectItem(root, "setting")){

                                if (cJSON_GetObjectItem(root, "value")){

                                    if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "thingname") == 0){

                                        esp_err_t err = change_setting(THINGNAME, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "Thing name chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "wifi_ssid") == 0){

                                        esp_err_t err = change_setting(WIFI_SSID, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "Wif SSID chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "wifi_pass") == 0){

                                        esp_err_t err = change_setting(WIFI_PASS, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "Wif PASS chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "ap_name") == 0){

                                        esp_err_t err = change_setting(AP_NAME, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "AP NAME chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "ap_pass") == 0){

                                        esp_err_t err = change_setting(AP_PASS, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "AP PASS chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "mqtt_host") == 0){

                                        esp_err_t err = change_setting(MQTT_HOST, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "MQTT_HOST chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    } else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "mqtt_port") == 0){

                                        uint32_t port = cJSON_GetObjectItem(root, "value")->valueint;
                                        esp_err_t err = change_setting(MQTT_PORT, &port);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "MQTT_PORT chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "pub_topic") == 0){

                                        esp_err_t err = change_setting(PUB_TOPIC, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "PUB_TOPIC chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "sub_topic") == 0){

                                        esp_err_t err = change_setting(SUB_TOPIC, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "SUB_TOPIC chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "info_topic") == 0){

                                        esp_err_t err = change_setting(INFO_TOPIC, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "INFO_TOPIC chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "log_topic") == 0){

                                        esp_err_t err = change_setting(LOG_TOPIC, cJSON_GetObjectItem(root, "value")->valuestring);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "PUB_TOPIC chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    } else if (strcmp(cJSON_GetObjectItem(root, "setting")->valuestring, "RELAY1") == 0){

                                        uint8_t relay1 = cJSON_GetObjectItem(root, "value")->valueint;
                                        esp_err_t err = change_setting(RELAY1, &relay1);
                                        if (err == ESP_OK){

                                            ESP_LOGI(TAG, "MQTT_PORT chaged succesfully");
                                            char *buff = get_settings();
                                            strncpy(resp_message.body, buff, strnlen(buff, CONFIG_MESSAGE_LENGTH));
                                            cJSON_free(buff);
                                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                                ESP_LOGI(TAG, "Queue response sent!");
                                            }
                                        }
                                    }
                                }
                                else{

                                    char *buff = "No value given ";
                                    strcpy(resp_message.body, buff);

                                    if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                        ESP_LOGI(TAG, "Queue response sent!");
                                    }
                                }
                            }
                        }
                        else{

                            char *buff = "No correct command given";
                            strcpy(resp_message.body, buff);

                            if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                                ESP_LOGI(TAG, "Queue response sent!");
                            }
                        }
                    }
                    else{

                        char *buff = "No correct ID and or JSON";
                        strcpy(resp_message.body, buff);

                        if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                            ESP_LOGI(TAG, "Queue response sent!");
                        }
                    }
                }
                else{

                    char *buff = "No correct ID and or JSON";
                    strcpy(resp_message.body, buff);

                    if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                        ESP_LOGI(TAG, "Queue response sent!");
                    }
                }
            }
            else{

                char *buff = "No correct ID and or JSON";
                strcpy(resp_message.body, buff);

                if (xQueueSend(recv_message.resp_queue, (void *)&resp_message, portMAX_DELAY)){

                    ESP_LOGI(TAG, "Queue response sent!");
                }
            }
            xSemaphoreGive(settings_mutex);
            ESP_LOGI(TAG, "settings_mutex given");
            cJSON_Delete(root);
        }
            //vTaskDelay(pdMS_TO_TICKS(10));
    }
}



esp_err_t parse_message(char* out_message, char* in_message, size_t len){
    
    
    if(len <= 0 || len > CONFIG_MESSAGE_LENGTH){
        ESP_LOGI(TAG, "Input message length is out of bounds, in_message length >= %d | in_message: \n\t%.*s", len, in_message, len );
        return ESP_FAIL;
    }


    char resp[CONFIG_MESSAGE_LENGTH];
    message_t message = {0};
    message_t resp_message = {0};
    message.resp_queue = xQueueCreate(1, sizeof(message_t));
    strncpy(message.body, in_message, len);
    
    if(xQueueSend(Message_queue, &message, portMAX_DELAY) != pdTRUE){
        ESP_LOGE(TAG, "Could't send message to parse!!");
        return ESP_FAIL;
    }
    
    xQueueReceive(message.resp_queue, (void *)&resp_message, portMAX_DELAY);
    strncpy(out_message, resp_message.body, strnlen(resp_message.body, CONFIG_MESSAGE_LENGTH));
    vQueueDelete(message.resp_queue);

    return ESP_OK;


}

void init_parser(const int task_size, app_settings_t* settings ){
    assert(settings);
    app_settings = settings;
    Message_queue = xQueueCreate(10, sizeof(message_t));
    xTaskCreate(message_parse, "message_parse",task_size, NULL, 15, NULL);
}