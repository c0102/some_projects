/*
 * Created on Thu Jun 16 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "enchele.h"
#include "parse.h"

extern SemaphoreHandle_t settings_mutex;

static const char* TAG = "MQTT";

static esp_mqtt_client_handle_t client; 
static app_settings_t* app_settings;

extern const uint8_t cacert_pem_start[] asm("_binary_cacert_pem_start");
extern const uint8_t cacert_pem_start[] asm("_binary_cacert_pem_start");

extern const uint8_t clientcert_crt_start[] asm("_binary_clientcert_crt_start");
extern const uint8_t clientcert_crt_end[]   asm("_binary_clientcert_crt_end");

extern const uint8_t privatekey_key_start[]   asm("_binary_privatekey_key_start");
extern const uint8_t privatekey_key_end[]   asm("_binary_privatekey_key_end");

#define WIFI_CONNECTED 1
#define WIFI_DISCONNECTED 0

static uint8_t conn = WIFI_DISCONNECTED;

esp_err_t send_message_mqtt_with_topic(char* data, char* topic){
    
    if ( esp_mqtt_client_publish(client, topic, data, 0, 0, 0) != -1){
        return ESP_OK;
    }else{
        return ESP_FAIL;
    }

    
}

esp_err_t send_message_mqtt(char* data){

    xSemaphoreTake(settings_mutex, portMAX_DELAY);
        esp_err_t err = send_message_mqtt_with_topic(data, app_settings->pub_topic);
    xSemaphoreGive(settings_mutex);

    return err;
}

static void wifi_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
        {
            conn = WIFI_CONNECTED;
            break;
        }
        
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            conn = WIFI_DISCONNECTED;
            break;
        }
        
        default:
        break;
    }
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;
    // message_t message = {0};
    // message_t resp_message = {0};
    // message.resp_queue = xQueueCreate(1, sizeof(message_t));

    char resp[CONFIG_MESSAGE_LENGTH];
    memset(resp, 0, CONFIG_MESSAGE_LENGTH);

    switch ((esp_mqtt_event_id_t)event_id) {

            case MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
                
                xSemaphoreTake(settings_mutex, portMAX_DELAY);
                    msg_id = esp_mqtt_client_subscribe(client, app_settings->sub_topic, 0);
                    
                    char buff[100];
                    memset(buff, 0, 100);
                    snprintf(buff, 100, "{\"message\":\"The module %s has connected to the mqtt broker!\"}", app_settings->thingname);
                    send_message_mqtt_with_topic(buff, app_settings->info_topic);

                xSemaphoreGive(settings_mutex);

                ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
                break;

            case MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
                break;

            case MQTT_EVENT_SUBSCRIBED:
                ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
                //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                break;

            case MQTT_EVENT_UNSUBSCRIBED:
                ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
                break;

            case MQTT_EVENT_PUBLISHED:
                ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
                break;

            case MQTT_EVENT_DATA:
                ESP_LOGI(TAG, "MQTT_EVENT_DATA");
                //printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
                //printf("DATA=%.*s\r\n", event->data_len, event->data);

                if(event->data_len > CONFIG_MESSAGE_LENGTH)
                {
                    ESP_LOGE(TAG, "The incoming message is too large to parse!");
                    break;
                }else
                {
                    ESP_ERROR_CHECK(parse_message(resp, event->data, event->data_len));
                    
                    // strncpy(message.body, (char*) event->data,event->data_len);
                    // if (xQueueSend(Message_queue, &message, portMAX_DELAY) != pdTRUE)
                    // {
                    //     ESP_LOGE(TAG, "Could't send message to parse!!");
                    //     break;
                    // }
                    // while (!xQueueReceive(message.resp_queue, (void *)&resp_message, pdMS_TO_TICKS(150)))
                    // {
                    //     ESP_LOGI(TAG, " Mqtt Message: Waiting for response");
                    // }


                    if(strlen(resp) <= 0){
                        break;
                    }else{
                        send_message_mqtt(resp);
                    }


                }

                break;

            case MQTT_EVENT_ERROR:
                ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                    ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                    ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                             strerror(event->error_handle->esp_transport_sock_errno));
                } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
                } else {
                    ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
                }
                break;

            default:
                ESP_LOGI(TAG, "Other event id:%d", event->event_id);
                break;
    }
    //vQueueDelete(message.resp_queue);
}
void main_mqtt_task(void* param){
    
    
}


void mqtt_init(app_settings_t* settings)
{
    assert(settings != NULL);
    app_settings = settings;

    xSemaphoreTake(settings_mutex, portMAX_DELAY);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = app_settings->mqtt_host,
        .port = app_settings->mqtt_port,
        //.transport = MQTT_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)cacert_pem_start,
        .client_cert_pem = (const char *)clientcert_crt_start,
        .client_key_pem = (const char *)privatekey_key_start,
    };
    xSemaphoreGive(settings_mutex);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    // while(1){
    //     vTaskDelay(pdMS_TO_TICKS(100));        
    // }
    
}