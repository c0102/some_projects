/*
 * Created on Thu Nov 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * 
 * Common functions, data structs, mutexes and other common, app-wide utilities
 */

#ifndef ENCHELE_COMMON_H
#define ENCHELE_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"



//NOTE(uran): maybe move this to menuconfig?
#define MAX_MESSAGE_LENGTH 1024

typedef struct{
    uint8_t ch1_pin;
    uint8_t ch2_pin;
    uint8_t ch3_pin;
    uint8_t ch4_pin;
    uint8_t sda_sht_pin;
    uint8_t scl_sht_pin;
    uint8_t rx_solar_pin;
    uint8_t beacon_pin;
    char thingname[64];
    char ap_name[CONFIG_AP_NAME_LENGTH];
    char ap_pass[CONFIG_AP_PASS_LENGTH];
    char mqtt_host[CONFIG_MQTT_HOST_URL_LENGTH];
    uint32_t mqtt_port;
    uint32_t slow_blink_intensity;
    uint32_t slow_blink_on_duration;
    uint32_t slow_blink_off_duration;
    uint32_t normal_blink_intensity;
    uint32_t normal_blink_on_duration;
    uint32_t normal_blink_off_duration;
    uint32_t fast_blink_intensity;
    uint32_t fast_blink_on_duration;
    uint32_t fast_blink_off_duration;
    uint32_t fault_blink_intensity;
    uint32_t fault_1_blink_on_duration;
    uint32_t fault_1_blink_off_duration;
    uint32_t fault_2_blink_on_duration;
    uint32_t fault_2_blink_off_duration;




    SemaphoreHandle_t mutex;

}app_settings_t;

#define FO_APP_CONFIG_DEFAULT_SETTINGS() {\
                                           .ch1_pin = CONFIG_CH_1_PIN,\
                                           .ch2_pin = CONFIG_CH_2_PIN,\
                                           .ch3_pin = CONFIG_CH_3_PIN,\
                                           .ch4_pin = CONFIG_CH_4_PIN,\
                                           .sda_sht_pin = CONFIG_SDA_I2C_SHT40,\
                                           .scl_sht_pin = CONFIG_SCL_I2C_SHT40,\
                                           .rx_solar_pin = CONFIG_UART_RX_SOLAR,\
                                           .beacon_pin = CONFIG_BEACON_PWM_PIN,\
                                           .ap_name = CONFIG_AP_NAME,\
                                           .ap_pass = CONFIG_AP_PASS,\
                                           .mqtt_host = CONFIG_MQTT_SERVER_HOST_URL,\
                                           .mqtt_port = CONFIG_MQTT_SERVER_HOST_PORT,\
                                           .thingname = CONFIG_THINGNAME_PREFIX "_" CONFIG_THINGNAME,\
                                           .slow_blink_intensity = CONFIG_SLOW_BLINK_INTENSITY,\
                                           .slow_blink_on_duration = CONFIG_SLOW_BLINK_ON_DURATION,\
                                           .slow_blink_off_duration = CONFIG_SLOW_BLINK_OFF_DURATION ,\
                                           .normal_blink_intensity = CONFIG_NORMAL_BLINK_INTENSITY,\
                                           .normal_blink_on_duration = CONFIG_NORMAL_BLINK_ON_DURATION,\
                                           .normal_blink_off_duration = CONFIG_NORMAL_BLINK_OFF_DURATION,\
                                           .fast_blink_intensity = CONFIG_FAST_BLINK_INTENSITY,\
                                           .fast_blink_on_duration = CONFIG_FAST_BLINK_ON_DURATION,\
                                           .fast_blink_off_duration = CONFIG_FAST_BLINK_OFF_DURATION,\
                                           .fault_blink_intensity = CONFIG_FAULT_BLINK_INTENSITY,\
                                           .fault_1_blink_on_duration = CONFIG_FAULT_1_BLINK_ON_DURATION,\
                                           .fault_1_blink_off_duration = CONFIG_FAULT_1_BLINK_OFF_DURATION,\
                                           .fault_2_blink_on_duration = CONFIG_FAULT_2_BLINK_ON_DURATION,\
                                           .fault_2_blink_off_duration = CONFIG_FAULT_2_BLINK_OFF_DURATION\
                                        }


//This struct is meant to store the data from both the sht40 sensor and the  victron solar charge controller
typedef struct{

    float temperature;
    float humidity;
    float bat_voltage;
    float bat_current;
    float load_current;
    float yield_today;
    float yield_yesterday;
    float panel_pwr;
    float panel_voltage;
       
    SemaphoreHandle_t mutex;
}sensor_values_t; 

 
typedef struct{
    uint32_t free_memory; //free heap memory in Kb
    uint32_t min_free_memory; //all time min memory
    bool day;
    bool changed;
    bool manual_override;
    char ap_ip[20];
    char eth_ip[20];
    char app_version[32];
    SemaphoreHandle_t mutex;
}system_state_t;

typedef enum{

    BEACON_OFF = 0,
    BEACON_ON,
    BLINK_SLOW,
    BLINK_NORMAL,
    BLINK_FAST,
    BLINK_FAULT_1,
    BLINK_FAULT_2,

}beacon_mode_t;

typedef struct{
    uint8_t ch1_state;
    uint8_t ch2_state;
    uint8_t ch3_state;
    uint8_t ch4_state;
    bool ch1_latch;
    bool ch2_latch;
    bool ch3_latch;
    bool ch4_latch;
    uint64_t ch1_time_on;
    uint64_t ch2_time_on;
    uint64_t ch3_time_on;
    uint64_t ch4_time_on;
    beacon_mode_t beacon_state;
    SemaphoreHandle_t mutex;
}outputs_state_t;

typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    char response[MAX_MESSAGE_LENGTH];
    SemaphoreHandle_t response_mutex;
}message_t;



void check_settings_and_update();
esp_err_t save_settings();
esp_err_t delete_settings_file();
void app_init(app_settings_t* settings,system_state_t* system_info, sensor_values_t* sensor_values, outputs_state_t* out_st );
esp_err_t updateSysInfo();
esp_err_t change_switch_state(uint8_t pin, uint8_t state);
//esp_err_t recieve_message(message_t* message);
void parse_switch_message(char* message);
void parse_manual_control_message(char* message);
void parse_state_message(char* message, int len);
void parse_data_message(char* message, int len);
void parse_in_state_message(char* message,int len);
void parse_settings_message(char* message,int len);
esp_err_t change_settings(char* message, int len);

#endif /* ENCHELE_COMMON_H */
