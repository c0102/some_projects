/*
 * Created on Fri Jun 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */


#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "string.h"
#include "enchele.h"
#include "hlw.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define HLW_DATA_SIZE 24

extern SemaphoreHandle_t settings_mutex;
static SemaphoreHandle_t hlw_mutex;
static QueueHandle_t uart1_queue;
//static intr_handle_t handle_console;
static hlw_raw_t hlw_raw = {0};
static hlw_data_t hlw_data_i = {0}; 

static app_settings_t* app_settings;



static const char* TAG = "HLW" ;


/*
    Checksum is compared to the sum of the values on the other registers excluding state_reg, check_reg and checksum_reg    
*/
bool check_checksum(){
    int sum = 0;
    for(int i = 2; i < 23; i++)
    {
        sum += hlw_raw.full[i];
    }
    ESP_LOGD(TAG, "[CHECKSUM] the sum is: %d and the checksum: %d", lowByte(sum), hlw_raw.full[23] );
    return hlw_raw.full[23] == lowByte(sum);
}

void set_hlw_0(){
    if (xSemaphoreTake(hlw_mutex, portMAX_DELAY)){
        hlw_data_i.voltage = 0;
        hlw_data_i.current = 0;
        hlw_data_i.active_power = 0;
        hlw_data_i.apparent_power = 0;
        hlw_data_i.power_factor = 0;

        xSemaphoreGive(hlw_mutex);

    }
}

void process_hlw_data(){
/*            uint8_t state_reg;
              uint8_t check_reg;
              uint8_t v_param[3];
              uint8_t v_reg[3];
              uint8_t c_param[3];
              uint8_t c_reg[3];
              uint8_t p_param[3];
              uint8_t p_reg[3];
              uint8_t data_up_reg;
              uint8_t pf_reg[2];
              uint8_t checksum;
*/
xSemaphoreTake(settings_mutex, portMAX_DELAY);
    float v_coeff = app_settings->v_coeff;
    float c_coeff = app_settings->c_coeff;
xSemaphoreGive(settings_mutex);

    if(hlw_raw.separate.state_reg != 0x55 && hlw_raw.separate.check_reg != 0x5a)
    {
        return;
    }
    if(check_checksum()){
        if (xSemaphoreTake(hlw_mutex, portMAX_DELAY))
        {
            static int64_t last_time = 0;

            // because apparently you can't initialize static vars with non constants ಠ╭╮ಠ
            if (last_time == 0)
            {
                last_time = esp_timer_get_time();
            }

            ESP_LOGD(TAG, "The v_param register full: %d", (uint32_t)(((uint32_t)hlw_raw.separate.v_param[0] << 16) + ((uint32_t)hlw_raw.separate.v_param[1] << 8) + hlw_raw.separate.v_param[2]));

            // The data update register tells us if the voltage and current have been updated , bit 4 for power, 5 for current, 6 for voltage
            if (bitRead(hlw_raw.separate.data_up_reg, 6) == 1)
            {
                uint32_t vol_param = (uint32_t)(((uint32_t)hlw_raw.separate.v_param[0] << 16) +
                                                ((uint32_t)hlw_raw.separate.v_param[1] << 8) + hlw_raw.separate.v_param[2]);

                uint32_t vol_reg = (uint32_t)(((uint32_t)hlw_raw.separate.v_reg[0] << 16) +
                                              ((uint32_t)hlw_raw.separate.v_reg[1] << 8) + hlw_raw.separate.v_reg[2]);

                
                hlw_data_i.voltage = vol_reg <= 0 ? 0.0 : (float)((float)vol_param / (float)vol_reg * v_coeff);
                
            }

            if (bitRead(hlw_raw.separate.data_up_reg, 5) == 1)
            {
                uint32_t curr_param = (uint32_t)(((uint32_t)hlw_raw.separate.c_param[0] << 16) +
                                                 ((uint32_t)hlw_raw.separate.c_param[1] << 8) + hlw_raw.separate.c_param[2]);

                uint32_t curr_reg = (uint32_t)(((uint32_t)hlw_raw.separate.c_reg[0] << 16) +
                                               ((uint32_t)hlw_raw.separate.c_reg[1] << 8) + hlw_raw.separate.c_reg[2]);

                xSemaphoreTake(settings_mutex, portMAX_DELAY);
                    hlw_data_i.current = curr_reg <= 0 ? 0.0 : (float)((float)curr_param / (float)curr_reg * c_coeff);
                xSemaphoreGive(settings_mutex);
            }

            if (bitRead(hlw_raw.separate.data_up_reg, 4) == 1)
            {
                uint32_t pow_param = (uint32_t)(((uint32_t)hlw_raw.separate.p_param[0] << 16) +
                                                ((uint32_t)hlw_raw.separate.p_param[1] << 8) + hlw_raw.separate.p_param[2]);
                uint32_t pow_reg = (uint32_t)(((uint32_t)hlw_raw.separate.p_reg[0] << 16) +
                                              ((uint32_t)hlw_raw.separate.p_reg[1] << 8) + hlw_raw.separate.p_reg[2]);

                xSemaphoreTake(settings_mutex, portMAX_DELAY);                          
                    hlw_data_i.active_power = pow_reg <= 0 ? 0.0 : (float)((float)pow_param / (float)pow_reg * c_coeff * v_coeff);
                xSemaphoreGive(settings_mutex);

                hlw_data_i.apparent_power = hlw_data_i.voltage * hlw_data_i.current;
                hlw_data_i.power_factor = hlw_data_i.apparent_power != 0 ? hlw_data_i.active_power / hlw_data_i.apparent_power : 0.0;
                hlw_data_i.active_energy += (hlw_data_i.active_power * ((esp_timer_get_time() - last_time) / 3600000000.0)) / 1000.0;
                hlw_data_i.apparent_energy += (hlw_data_i.apparent_power * ((esp_timer_get_time() - last_time) / 3600000000.0)) / 1000.0;
                last_time = esp_timer_get_time();
            }

            ESP_LOGD(TAG, "Voltage: %.02f V  Current: %.02f A  Active Pow: %.02f W  "
                          "Apparent Pow: %.02f VA  Active Energy: %f KWh  Apparent Energy: %f KWh",
                     hlw_data_i.voltage, hlw_data_i.current, hlw_data_i.active_power, hlw_data_i.apparent_power,
                     hlw_data_i.active_energy, hlw_data_i.apparent_energy);

            xSemaphoreGive(hlw_mutex);
        }
    }

}

void hlw_task(void *params)
{
    uart_event_t event;
    static uint64_t lastTime = 0;
    
    while(1)
    {
        if(xQueueReceive(uart1_queue, (void * )&event, pdMS_TO_TICKS(150))) {
            ESP_LOGD(TAG, "uart[%d] event:", UART_NUM_1);
            switch(event.type) {
                case UART_BREAK:
                    ESP_LOGI(TAG,"[UART EVENT] time out");
                    uart_flush_input(UART_NUM_1);
                    lastTime = esp_timer_get_time();
                    set_hlw_0();
                    break;

                case UART_DATA:
                    ESP_LOGD(TAG, "[UART DATA]: %d", event.size);
                    uart_read_bytes(UART_NUM_1, &hlw_raw, event.size, portMAX_DELAY);
                    
                    lastTime = esp_timer_get_time();

                    ESP_LOGD(TAG,"Data recieved: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                                                 hlw_raw.separate.state_reg,
                                                 hlw_raw.separate.check_reg,
                                                 hlw_raw.separate.v_param[0],
                                                 hlw_raw.separate.v_param[1],
                                                 hlw_raw.separate.v_param[2],
                                                 hlw_raw.separate.v_reg[0],
                                                 hlw_raw.separate.v_reg[1],
                                                 hlw_raw.separate.v_reg[2],
                                                 hlw_raw.separate.c_param[0],
                                                 hlw_raw.separate.c_param[1],
                                                 hlw_raw.separate.c_param[2],
                                                 hlw_raw.separate.c_reg[0],
                                                 hlw_raw.separate.c_reg[1],
                                                 hlw_raw.separate.c_reg[2],
                                                 hlw_raw.separate.p_param[0],
                                                 hlw_raw.separate.p_param[1],
                                                 hlw_raw.separate.p_param[2],
                                                 hlw_raw.separate.p_reg[0],
                                                 hlw_raw.separate.p_reg[1],
                                                 hlw_raw.separate.p_reg[2],
                                                 hlw_raw.separate.data_up_reg,
                                                 hlw_raw.separate.pf_reg[0],
                                                 hlw_raw.separate.pf_reg[1],
                                                 hlw_raw.separate.checksum);
                    
                    if(hlw_raw.separate.state_reg != 85 && hlw_raw.separate.check_reg != 90)
                    {
                        uart_flush_input(UART_NUM_1);
                        break;
                    }
                    
                    process_hlw_data();
                    break;

                default:
                    ESP_LOGI(TAG, "[UART EVENTS] The default case!");
                    uart_flush_input(UART_NUM_1);
                    lastTime = esp_timer_get_time();
                    set_hlw_0();
                    break;
            }

        }else{ 
            ESP_LOGD(TAG, "[HLW TASK] no queue event, time passed: %llu", esp_timer_get_time() - lastTime);
            if(esp_timer_get_time() - lastTime > 2000){
                set_hlw_0();
            }
        }
    }
}


esp_err_t hlw_init(app_settings_t* settings){

    ESP_LOGD(TAG, "Initializing uart1 for hlw");
    app_settings = settings;
    hlw_mutex = xSemaphoreCreateMutex();

    uart_config_t uart_config = {
        .baud_rate = 4800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        
    };

    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_install(UART_NUM_1, 130 , 0, 10, &uart1_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    
    xSemaphoreTake(settings_mutex, portMAX_DELAY);
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, app_settings->HLW, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xSemaphoreGive(settings_mutex);
    
    uart_intr_config_t uart_intr = {
        .intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M,
        .rxfifo_full_thresh = 24,
        .rx_timeout_thresh = 110,
    };

    ESP_ERROR_CHECK(uart_intr_config(UART_NUM_1, &uart_intr));
    ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_1));
    ESP_ERROR_CHECK(uart_enable_intr_mask(UART_NUM_1, UART_RXFIFO_FULL_INT_ENA_M ));
    uart_set_always_rx_timeout(UART_NUM_1, true);
    xTaskCreate(&hlw_task, "hlw_task", 1024*5, NULL, 5, NULL);
    return ESP_OK;
}

hlw_data_t get_hlw_data(void){
    hlw_data_t temp = {0};

    if(xSemaphoreTake(hlw_mutex, pdMS_TO_TICKS(200)) != pdTRUE){
        return temp;
    }
    temp = hlw_data_i;
    xSemaphoreGive(hlw_mutex);
    return temp;
}
