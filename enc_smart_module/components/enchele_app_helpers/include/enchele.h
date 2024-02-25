/*
 * Created on Sun Jun 12 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#ifndef ENCHELE_ENCHELE
#define ENCHELE_ENCHELE
#include "stdint.h"
#include "esp_err.h"

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))


/*
    The Kconfig system doesn't allow for floating point numbers to be defined 
    so we are defining everything that is not included in the Kconfig build 
*/
#define CONFIG_HLW_V_COEFF 1.88  //the coeficient of voltage for hlw, check the datasheet for more info
#define CONFIG_HLW_C_SHUNT 0.001 //the value of the shunt resistor for the hlw, to calculate the current coeff. check the datasheet

typedef enum
{
    THINGNAME = 0,
    WIFI_SSID,
    WIFI_PASS,
    AP_NAME,
    AP_PASS,
    MQTT_HOST,
    MQTT_PORT,
    PUB_TOPIC,
    SUB_TOPIC,
    INFO_TOPIC,
    LOG_TOPIC,
    RELAY1,
    RELAY2,
    HLW,
    V_COEFF,
    C_SHUNT,
    C_COEFF,
    
}app_settings_n;

typedef struct {
    char thingname[CONFIG_THINGNAME_LENGTH];
    char wifi_ssid[CONFIG_WIFI_SSID_LENGTH];
    char wifi_pass[CONFIG_WIFI_PASS_LENGTH];
    char ap_name[CONFIG_AP_NAME_LENGTH];
    char ap_pass[CONFIG_AP_PASS_LENGTH];
    char mqtt_host[CONFIG_MQTT_HOST_URL_LENGTH];
    uint32_t mqtt_port; 
    char pub_topic[CONFIG_MQTT_TOPIC_LENGTH];
    char sub_topic[CONFIG_MQTT_TOPIC_LENGTH];
    char info_topic[CONFIG_MQTT_TOPIC_LENGTH];
    char log_topic[CONFIG_MQTT_TOPIC_LENGTH];
    uint8_t RELAY1;
    uint8_t SWITCH1;
#ifdef CONFIG_DUAL_MODULE
    uint8_t RELAY2;
    uint8_t SWITCH2;
#endif
    uint8_t HLW;
    float v_coeff;
    float c_shunt;
    float c_coeff;
}app_settings_t;

#ifdef CONFIG_DUAL_MODULE
#define ENCHELE_CONFIG_DEFAULT_SETTINGS() { \
                                           .thingname = CONFIG_THINGNAME_PREFIX "_" CONFIG_THINGNAME, \
                                           .wifi_ssid = CONFIG_WIFI_SSID,\
                                           .wifi_pass = CONFIG_WIFI_PASS,\
                                           .ap_name = CONFIG_AP_NAME,\
                                           .ap_pass = CONFIG_AP_PASS,\
                                           .mqtt_host = CONFIG_MQTT_SERVER_HOST_URL,\
                                           .mqtt_port = CONFIG_MQTT_SERVER_HOST_PORT,\
                                           .pub_topic = CONFIG_PUBLISH_TOPIC,\
                                           .sub_topic = CONFIG_SUBSCRIBE_TOPIC,\
                                           .info_topic = CONFIG_INFO_TOPIC,\
                                           .log_topic = CONFIG_LOG_TOPIC,\
                                           .RELAY2 = CONFIG_RELAY_PIN_2,\
                                           .RELAY1 = CONFIG_RELAY_PIN,\
                                           .SWITCH1 = CONFIG_SWITCH_PIN,\
                                           .SWITCH2 = CONFIG_SWITCH_PIN_2,\ 
                                           .HLW = CONFIG_HLW_PIN,\
                                           .v_coeff = CONFIG_HLW_V_COEFF,\
                                           .c_shunt = CONFIG_HLW_C_SHUNT,\
                                           .c_coeff = (float)((float)1 / (float)(CONFIG_HLW_C_SHUNT * 1000 )) }// from the datasheet
                                           
#else
#define ENCHELE_CONFIG_DEFAULT_SETTINGS() { \
                                           .thingname = CONFIG_THINGNAME_PREFIX "_" CONFIG_THINGNAME, \
                                           .wifi_ssid = CONFIG_WIFI_SSID,\
                                           .wifi_pass = CONFIG_WIFI_PASS,\
                                           .ap_name = CONFIG_AP_NAME "_" CONFIG_THINGNAME,\
                                           .ap_pass = CONFIG_AP_PASS,\
                                           .mqtt_host = CONFIG_MQTT_SERVER_HOST_URL,\
                                           .mqtt_port = CONFIG_MQTT_SERVER_HOST_PORT,\
                                           .pub_topic = CONFIG_PUBLISH_TOPIC,\
                                           .sub_topic = CONFIG_SUBSCRIBE_TOPIC,\
                                           .info_topic = CONFIG_INFO_TOPIC,\
                                           .log_topic = CONFIG_LOG_TOPIC,\
                                           .RELAY1 = CONFIG_RELAY_PIN,\
                                           .SWITCH1 = CONFIG_SWITCH_PIN,\
                                           .HLW = CONFIG_HLW_PIN,\
                                           .v_coeff = CONFIG_HLW_V_COEFF,\
                                           .c_shunt = CONFIG_HLW_C_SHUNT,\
                                           .c_coeff = (float)((float)1 / (float)(CONFIG_HLW_C_SHUNT * 1000 ))}  // from the datasheet




esp_err_t change_setting(app_settings_n setting, void* value );

void get_current_time(char* time_string);

void app_init();


#endif

#endif /* ENCHELE_ENCHELE */
