/*
 * Created on Thu Nov 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "string.h"
#include "esp_log.h"
#include "sht4x.h"
#include "common.h"
#include "FO_http_server.h"
#include "sht40_front.h"


static app_settings_t* app_settings;
static sensor_values_t* sensor_values;


static const char* TAG = "SHT40_FRONT";

static sht4x_t dev = {};



void main_sht40_task(void* param)
{
    while(1){
        if(xSemaphoreTake(sensor_values->mutex, pdMS_TO_TICKS(500)) == pdTRUE){

            sht4x_measure(&dev, &sensor_values->temperature, &sensor_values->humidity);
             xSemaphoreGive(sensor_values->mutex);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }else{
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    
    }
}

esp_err_t sht40_init(app_settings_t* settings, sensor_values_t* sensor_val){

    app_settings = settings;
    sensor_values = sensor_val;

    ESP_ERROR_CHECK(i2cdev_init());
    memset(&dev, 0, sizeof(sht4x_t));
    esp_err_t device_check_res;

xSemaphoreTake(app_settings->mutex, portMAX_DELAY);
    ESP_ERROR_CHECK(sht4x_init_desc(&dev, I2C_NUM_1, app_settings->sda_sht_pin, app_settings->scl_sht_pin));
xSemaphoreGive(app_settings->mutex);

    device_check_res = i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE);
    if(device_check_res == ESP_OK){
        ESP_ERROR_CHECK(sht4x_init(&dev));
        ESP_LOGI(TAG, "sht4x init sucess!");
        xTaskCreatePinnedToCore(main_sht40_task, "main_sht40_task", 1024*5, NULL, 9, NULL, 1);
        return ESP_OK;
    }else{
        ESP_LOGI(TAG, "SHT4x device not connected!");
        return ESP_FAIL;
    }
}



// esp_err_t sht30_init(app_settings_t* settings, sensor_values_t* sensor_val){

//     app_settings = settings;
//     sensor_values = sensor_val;

//     ESP_ERROR_CHECK(i2cdev_init());
//     memset(&dev, 0, sizeof(sht4x_t));
//     esp_err_t device_check_res;

// xSemaphoreTake(app_settings->mutex, portMAX_DELAY);
//     ESP_ERROR_CHECK(sht3x_init_desc(&dev, 0x45, I2C_NUM_1, app_settings->sda_sht_pin, app_settings->scl_sht_pin));
// xSemaphoreGive(app_settings->mutex);

//     device_check_res = i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE);
//     if(device_check_res == ESP_OK){
//         ESP_ERROR_CHECK(sht3x_init(&dev));
//         ESP_LOGI(TAG, "sht3x init sucess!");
//         xTaskCreatePinnedToCore(main_sht40_task, "main_sht40_task", 1024*5, NULL, 9, NULL, 1);
//         return ESP_OK;
//     }else{
//         ESP_LOGI(TAG, "SHT3x device not connected!");
//         return ESP_FAIL;
//     }
// }

