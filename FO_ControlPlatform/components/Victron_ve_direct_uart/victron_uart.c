/*
 * Created on Mon Nov 28 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "common.h"
#include "victron_uart.h"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO

static const char* TAG = "VBS_UART";

static app_settings_t* app_settings;
static sensor_values_t* sensor_values;
static QueueHandle_t uart1_queue;
static TaskHandle_t receiverHandler = NULL;
static char buffer[VED_BUFF_SIZE+1];

void process_data(void *params){

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        char* line, *label, *value;
        // ESP_LOGD(TAG, "notification taken");
        char temp_buff[VED_BUFF_SIZE] ;
        memset(temp_buff, 0, VED_BUFF_SIZE);
        strncpy(temp_buff, buffer, strnlen(buffer, VED_BUFF_SIZE));
        
        ESP_LOGD(TAG, "Checking the checksum!");
        int8_t checksum = 0;
        for(int i = 0; i < strnlen(temp_buff, VED_BUFF_SIZE); i++){
            checksum += temp_buff[i];
            //printf("%x ", temp_buff[i]);
        }

        //printf("\n checksum: %d\n", checksum);
        
        if(checksum){
            ESP_LOGD(TAG, "The data is not correct, checksum failed! ");
            //continue;
        }
        char* save_field = "field";
        char* save_line = "line";

        line  = strtok_r(temp_buff, "\n", &save_field);
        while(line != NULL){
            label = strtok_r(line, "\t", &save_line);
            value = strtok_r(0, "\t", &save_line);
            //ESP_LOGD(TAG, "label: %s , value: %s", label, value);
            if(label != NULL && value != NULL){
                if(strcmp(label, "V") == 0 ){
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->bat_voltage = atof(value) / 1000.0; //the value arrives as mV so convert it to V
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "I") == 0 ){
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->bat_current = atof(value) / 1000.0; //the value arrives as mA so convert it to A
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "VPV") == 0 ){
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       //ESP_LOGI(TAG, "Panel Voltage : %s mV", value);
                       sensor_values->panel_voltage = atof(value) / 1000.0; //the value arrives as mV so convert it to V
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "PPV") == 0 ){
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->panel_pwr = atof(value);
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "IL") == 0 ){
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->load_current = atof(value) / 1000.0; //the value arrives as mA so convert it to A
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "H20") == 0 ){
                    //ESP_LOGI(TAG, "Yield Today : %s", value);
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->yield_today = atoi(value) * 10; //The value arrives as 10Wh so convert to Wh
                    xSemaphoreGive(sensor_values->mutex);                    
                }else if(strcmp(label, "H22") == 0 ){
                    //ESP_LOGI(TAG, "Yield Yesterday : %s", value);
                    xSemaphoreTake(sensor_values->mutex, portMAX_DELAY);
                       sensor_values->yield_yesterday = atoi(value) * 10; //The value arrives as 10Wh so convert to Wh
                    xSemaphoreGive(sensor_values->mutex);                    
                }
            }
            line = strtok_r(0, "\n", &save_field);
        }

    }
}

void vbs_task(void *params)
{
    uart_event_t event;
    //static uint64_t lastTime = 0;

    while(1)
    {   
        if(xQueueReceive(uart1_queue, (void * )&event, pdMS_TO_TICKS(150))) {
            ESP_LOGD(TAG, "uart[%d] event: %d", UART_NUM_1, event.type);
            memset(buffer, 0 , VED_BUFF_SIZE);
            switch(event.type) {
                case UART_DATA:{
                    ESP_LOGD(TAG, "[UART DATA]: %d", event.size);
                    if(event.size > VED_BUFF_SIZE){
                        ESP_LOGE(TAG, "Recieved data is longer than buffer!");
                        break;
                    }
                    uart_read_bytes(UART_NUM_1, buffer, event.size, 1000/portTICK_PERIOD_MS);
                    //printf("%.*s", strnlen(buffer, VED_BUFF_SIZE), buffer ); //just a C annoyance, we have to defend against missing null terminator
                    xTaskNotifyGive(receiverHandler);
                    
                    
                }
                default:
                    break;
            }
        }
    }
}

esp_err_t vbs_init(app_settings_t* settings, sensor_values_t* sens_val){

    ESP_LOGD(TAG, "Initializing uart1 for Victron BlueSolar device");
    app_settings = settings;
    sensor_values = sens_val;

    uart_config_t uart_config = { //https://www.victronenergy.com/live/vedirect_protocol:faq
        .baud_rate = VED_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,

    };

    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_install(UART_NUM_1, VED_BUFF_SIZE*2 , 0, 10, &uart1_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    xSemaphoreTake(app_settings->mutex, portMAX_DELAY);
        //Use uart1, set rx pin to CONFIG value and leave the rest as default
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, app_settings->rx_solar_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xSemaphoreGive(app_settings->mutex);

    // uart_intr_config_t uart_intr = {

    //     .intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M,
    //     .rxfifo_full_thresh = 24,
    //     .rx_timeout_thresh = 110,
    // };

    // ESP_ERROR_CHECK(uart_intr_config(UART_NUM_1, &uart_intr));
    // ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_1));
    // ESP_ERROR_CHECK(uart_enable_intr_mask(UART_NUM_1, UART_RXFIFO_FULL_INT_ENA_M ));
    // uart_set_always_rx_timeout(UART_NUM_1, true);

    memset(buffer, 0, VED_BUFF_SIZE+1);
    xTaskCreatePinnedToCore(&process_data, "process_data", 1024*3, NULL, 9, &receiverHandler, 1);
    xTaskCreatePinnedToCore(&vbs_task, "vbs_task", 1024*5, NULL, 9, NULL, 1);
 

    return ESP_OK;
}