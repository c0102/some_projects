#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/apps/sntp.h" //zurb
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"

//#include "tcpip_adapter.h"

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>

#include "nvs_flash.h"
#include "mqtt_client.h"

#include "cJSON.h"

//#define ATECC608A_CRYPTO_ENABLED
#ifdef ATECC608A_CRYPTO_ENABLED
#include "cryptoauthlib.h"
#endif

#define RS485_DEVICES_NUM (16)

#define MY_LOGI(tag, format, ...) do {               \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO,   tag, format, ##__VA_ARGS__); \
        if (settings.mqtt1_loglevel>=ESP_LOG_INFO ) MQTT_LOG(ESP_LOG_INFO, 0, tag, format, ##__VA_ARGS__); \
        if (settings.mqtt2_loglevel>=ESP_LOG_INFO ) MQTT_LOG(ESP_LOG_INFO, 1, tag, format, ##__VA_ARGS__); \
    } while(0)

#define MY_LOGE(tag, format, ...) do {               \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR,   tag, format, ##__VA_ARGS__); \
        if (settings.mqtt1_loglevel>=ESP_LOG_ERROR ) MQTT_LOG(ESP_LOG_ERROR, 0, tag, format, ##__VA_ARGS__); \
        if (settings.mqtt2_loglevel>=ESP_LOG_ERROR ) MQTT_LOG(ESP_LOG_ERROR, 1, tag, format, ##__VA_ARGS__); \
    } while(0)

#define MY_LOGW(tag, format, ...) do {               \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN,   tag, format, ##__VA_ARGS__); \
        if (settings.mqtt1_loglevel>=ESP_LOG_WARN ) MQTT_LOG(ESP_LOG_WARN, 0, tag, format, ##__VA_ARGS__); \
        if (settings.mqtt2_loglevel>=ESP_LOG_WARN ) MQTT_LOG(ESP_LOG_WARN, 1, tag, format, ##__VA_ARGS__); \
    } while(0)

#define MY_LOGD(tag, format, ...) do {               \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG,   tag, format, ##__VA_ARGS__); \
        if (settings.mqtt1_loglevel>=ESP_LOG_DEBUG ) MQTT_LOG(ESP_LOG_DEBUG, 0, tag, format, ##__VA_ARGS__); \
        if (settings.mqtt2_loglevel>=ESP_LOG_DEBUG ) MQTT_LOG(ESP_LOG_DEBUG, 1, tag, format, ##__VA_ARGS__); \
    } while(0)

/******************************************************************************/
/* SMART GATEWAY BOARD CONFIGURATION */
/******************************************************************************/
#if CONFIG_BOARD_SG
//#define TEST_IMMUNITY
#define ENABLE_ETHERNET
#define ENABLE_RS485_PORT
#define BICOM_RS485_ENABLED
#define ENABLE_LEFT_IR_PORT
#define ENABLE_RIGHT_IR_PORT
#define LED_ENABLED
#define WEB_SERVER_ENABLED
#define MODBUS_TCP_SERVER
#define UDP_SERVER
#define NTP_ENABLED
#define START_CONTROL_TASK
#define START_MQTT_TASK
#define FACTORY_RESET_ENABLED
#define ENABLE_PT1000
#define ENABLE_PULSE_COUNTER
#define DISPLAY_STACK_FREE
#define TCP_CLIENT_ENABLED
#define MDNS_ENABLED
#define EXTERNAL_WATCHDOG_ENABLED
#define EEPROM_STORAGE
#define STATIC_IP
//#define DEMO_BOX

#define DEFAULT_MODEL_TYPE	"SG-W1           "
#define MODEL_TYPE_W1	 	"SG-W1           "
#define MODEL_TYPE_W1A	 	"SG-W1A          "
#define MODEL_TYPE_E1		"SG-E1           "

#define SG_MODBUS_ADDRESS	(34)
#define SG_MODBUS_TCP_PORT	(10001)

//uarts
#define UART_0_TXD	(1)
#define UART_0_RXD	(3)
#define RS485_UART_TXD	(UART_0_TXD)
#define RS485_UART_RXD	(UART_0_RXD)
#define RS485_UART_PORT	(UART_NUM_0)

#define UART_1_TXD	(14)
#define UART_1_RXD	(13)
#define RIGHT_IR_UART_PORT	(UART_NUM_1)
#define RIGHT_IR_UART_TXD	(UART_1_TXD)
#define RIGHT_IR_UART_RXD	(UART_1_RXD)

#define UART_2_TXD	(15)
#define UART_2_RXD	(16)
#define UART_2_RTS	UART_PIN_NO_CHANGE
#define LEFT_IR_UART_PORT	(UART_NUM_2)
#define LEFT_IR_UART_TXD	(UART_2_TXD)
#define LEFT_IR_UART_RXD	(UART_2_RXD)

//leds
#define LED_PIN_RED		(33)
#define LED_PIN_GREEN	(32)
#define LED_RED			(0)
#define LED_GREEN		(1)
#define LED_ORANGE		(2)

#endif //CONFIG_BOARD_SG

/******************************************************************************/
/* IMPACT IE14MW BOARD CONFIGURATION */
/******************************************************************************/
#if CONFIG_BOARD_IE14MW
//#define ENABLE_ETHERNET
#define ENABLE_RS485_PORT
//#define ENABLE_LEFT_IR_PORT
//#define ENABLE_RIGHT_IR_PORT
//#define LED_ENABLED
#define WEB_SERVER_ENABLED
#define MODBUS_TCP_SERVER
#define UDP_SERVER
#define NTP_ENABLED
#define START_CONTROL_TASK
//#define START_MQTT_TASK
//#define FACTORY_RESET_ENABLED
//#define ENABLE_PT1000
//#define ENABLE_PULSE_COUNTER
#define MDNS_ENABLED
#define DISPLAY_STACK_FREE

#define DEFAULT_MODEL_TYPE "IE14MW         "   /* zurb fill with spaces */

//uarts
#define UART_2_TXD	(17)
#define UART_2_RXD	(16)
#define UART_2_RTS	UART_PIN_NO_CHANGE
#define RS485_UART_TXD	(UART_2_TXD)
#define RS485_UART_RXD	(UART_2_RXD)
#define RS485_UART_PORT	(UART_NUM_2)

//leds
#define LED_PIN_RED		(33)
#define LED_PIN_GREEN	(32)
#define LED_RED			(0)
#define LED_GREEN		(1)
#define LED_ORANGE		(2)

#endif //CONFIG_BOARD_IE14MW

/******************************************************************************/
/* OLIMEX ESP32-GATEWAY BOARD CONFIGURATION */
/******************************************************************************/
#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
#define ENABLE_ETHERNET
#define ENABLE_RS485_PORT
//#define ENABLE_LEFT_IR_PORT
//#define ENABLE_RIGHT_IR_PORT
#define LED_ENABLED
#define WEB_SERVER_ENABLED
#define MODBUS_TCP_SERVER
#define UDP_SERVER
#define NTP_ENABLED
#define START_CONTROL_TASK
#define START_MQTT_TASK
//#define FACTORY_RESET_ENABLED
//#define ENABLE_PT1000
//#define ENABLE_PULSE_COUNTER
#define MDNS_ENABLED
#define DISPLAY_STACK_FREE

#define DEFAULT_MODEL_TYPE "OLIMEX_ESP32_GW"   /* zurb fill with spaces */

//uarts
// Note: UART2 default pins IO16, IO17
#define UART_2_TXD   (17)
#define UART_2_RXD   (16)

// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define UART_2_RTS   (32)
#define GPIO_UART2_RTS_OUTPUT_PIN_SEL  (1ULL<<UART_2_RTS)
// CTS is not used in RS485 Half-Duplex Mode
//#define UART_2_CTS  UART_PIN_NO_CHANGE

#define RS485_UART_TXD	(UART_2_TXD)
#define RS485_UART_RXD	(UART_2_RXD)
#define RS485_UART_PORT	(UART_NUM_2)

//leds
#define LED_PIN_RED (2)
#define LED_RED		(0)
#define LED_GREEN	(0)
#define LED_ORANGE	(0)

#endif //CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
/******************************************************************************/

/******************************************************************************/
/* ESP32 DEMO BOARD CONFIGURATION */
/******************************************************************************/
#if CONFIG_BOARD_ESP32_DEMO
//#define ENABLE_ETHERNET
//#define ENABLE_RS485_PORT
//#define ENABLE_LEFT_IR_PORT
//#define ENABLE_RIGHT_IR_PORT
#define LED_ENABLED
#define WEB_SERVER_ENABLED
//#define MODBUS_TCP_SERVER
#define UDP_SERVER
#define NTP_ENABLED
#define START_CONTROL_TASK
//#define START_MQTT_TASK
//#define FACTORY_RESET_ENABLED
//#define ENABLE_PT1000
//#define ENABLE_PULSE_COUNTER
#define MDNS_ENABLED
#define DISPLAY_STACK_FREE

#define DEFAULT_MODEL_TYPE "ESP32_DEMO     "   /* zurb fill with spaces */
//leds
#define LED_PIN_RED (2)
#define LED_RED		(0)
#define LED_GREEN	(0)
#define LED_ORANGE	(0)

#endif //CONFIG_BOARD_ESP32_DEMO
/******************************************************************************/

//#define SECURE_HTTP_SERVER
#define HTTPD_STACK_SIZE (1024*7)
#define WEBSERVER_THREAD_STACKSIZE (1024*3)
#define WEBSERVER_THREAD_PRIO 10

#define CONNECT_TIMEOUT (180)
#define WIFI_RETRY_LIMIT     (100)

#define CONNECTED_BIT	( 1 << 1 )
#define GOT_IP_BIT		( 1 << 2 )
#define HTTP_STOP		( 1 << 3 )
#define MQTT_STOP		( 1 << 4 )
#define TCP_SERVER_STOP	( 1 << 5 )
#define BICOM_TASK_STOP	( 1 << 6 )
#define ADC_TASK_STOP	( 1 << 7 )
#define COUNT_TASK_STOP	( 1 << 8 )
#define MODBUS_SLAVE_STOP	( 1 << 9 )
#define FACTORY_RESET_END	( 1 << 10 )

#define CONFIG_MQTT_BUFFER_SIZE		(1024*4)
#define MQTT_INTERNAL_BUFFER_SIZE	(1024*4)
#define UPDATE_URL_SIZE				(100)

#define	TCP_SERVER_PORT		(10001)
#define	TCP_SERVER_BUFFER	(1024)
#define TCP_MODBUS_HEADER_SIZE (6)

#define JSON_STATUS_SIZE (700)
#define JSON_SETTINGS_SIZE (1024*5)

#define ETHERNET_MODE	(1)
#define WIFI_MODE		(2)

#define RS485_ENERGY_METER		(2)
#define RS485_BISTABILE_SWITCH	(3)
#define RS485_PQ_METER			(4)

#ifdef FACTORY_RESET_ENABLED
#define FACTORY_RESET_TIMEOUT (10) //10 seconds
#define FACTORY_RESET_NVS_KEY "Factory_reset"
#endif

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
#define BLE_PROV_NVS_KEY "Ble_prov_start"
#define BLE_PROV_MANAGER_TIMEOUT (5*60*1000*1000U) //5 minutes
#endif

#define STATUS_NO_CONNECTION		(0)
#define STATUS_CONNECTING			(1)
#define STATUS_PROVISIONING			(2)
#define STATUS_WIFI_CONNECTED		(3)
#define STATUS_ETHERNET_CONNECTED	(4)

#define PUSH_DISABLED	(0)
#define MQTT_PROTOCOL	(1)
#define MISMART_PUSH	(2)

//log levels
//register 40013
#define ADC_DEBUG			BIT0
#define ATEC_DEBUG			BIT1
#define BICOM_DEBUG			BIT2
#define CONTROL_DEBUG		BIT3
#define DIG_INPUT_DEBUG		BIT4
#define ENERGY_METER_DEBUG	BIT5
#define ETH_INIT_DEBUG		BIT6
#define GRAPH_DEBUG			BIT7
#define HTTP_SERVER_DEBUG	BIT8
#define LED_DEBUG			BIT9
#define LEFT_IR_DEBUG		BIT10
#define MAIN_DEBUG			BIT11
#define MODBUS_CLIENT_DEBUG BIT12
#define MODBUS_SLAVE_DEBUG	BIT13
#define MQTT_DEBUG			BIT14
#define NTP_DEBUG			BIT15

//register 40014
#define NVS_DEBUG			BIT0
#define OTA_DEBUG			BIT1
#define PUSH_DEBUG			BIT2
#define RS485_DEBUG			BIT3
#define TCP_CLIENT_DEBUG	BIT4
#define TCP_SERVER_DEBUG	BIT5
#define UDP_LOG_DEBUG		BIT6
#define UTILS_DEBUG			BIT7
#define WIFI_INIT_DEBUG		BIT8

//error flags
#define ERR_NVS_SETTINGS BIT0
#define ERR_SETTINGS_CRC BIT1
#define ERR_EEPROM		 BIT2
#define ERR_MQTT_INIT_1	 BIT3
#define ERR_MQTT_INIT_2	 BIT4

#define NUMBER_OF_RECORDERS (4)

extern EventGroupHandle_t ethernet_event_group;
extern EventGroupHandle_t uart_event_group;

#ifdef ENABLE_RS485_PORT
typedef struct RS485devicesStruct {
	int8_t type;
	uint8_t address;
	char description[20];

}RS485devicesStruct_t;
#endif

typedef struct pushPublishServerStruct {
	//int8_t push_enabled;
	//uint16_t push_interval;
	uint8_t push_protocol;
	int8_t push_resp_time;
	char mqtt_server[60];
	int mqtt_port;
	char mqtt_cert_file[32]; //mqtt server certificate file
	char mqtt_client_cert_file[32]; //mqtt client certificate file
	char mqtt_key_file[32]; //mqtt client private key file
	char mqtt_root_topic[32];
	char mqtt_sub_topic[32];
	char mqtt_pub_topic[32];
	char mqtt_username[33];
	char mqtt_password[33];
	uint8_t mqtt_qos; //Quality of service
	uint8_t mqtt_tls;
}pushPublishServerStruct_t;

typedef struct pushPublishDeviceStruct {
	int8_t push_link;
	uint16_t push_interval;
}pushPublishDeviceStruct_t;

//settings, dont forget to set default setting in nvc_storage.c !!!
typedef struct settingsStruct {
	uint8_t locked; //WEB settings lock
	char description[40];
	char location[40];
	char wifi_ssid[20];
	char wifi_password[20];
	char ntp_server1[40];
	char ntp_server2[40];
	char ntp_server3[40];
	pushPublishServerStruct_t publish_server[2];
	int8_t connection_mode; //wifi or ethernet
	int8_t wifi_max_retry;
	uint8_t sg_modbus_addr;
	uint16_t tcp_port;
	uint16_t http_port;
	//uint8_t debug_console;
	int16_t timezone;
	uint8_t time_sync_src;
#ifdef ENABLE_LEFT_IR_PORT
	int8_t left_ir_enabled;
	int8_t ir_counter_addr;
	pushPublishDeviceStruct_t left_ir_push;
	char left_ir_desc[20];
#endif
#ifdef ENABLE_RIGHT_IR_PORT
	int8_t ir_ext_rel_mode;
	char ir_relay_description[20];
	pushPublishDeviceStruct_t right_ir_push;
#endif
#ifdef ENABLE_RS485_PORT
	unsigned short rs485_baud_rate; //40422
	unsigned short rs485_stop_bits; //40423
	unsigned short rs485_parity;    //40424
	unsigned short rs485_data_bits; //40425
	RS485devicesStruct_t rs485[RS485_DEVICES_NUM];
	pushPublishDeviceStruct_t rs485_push[RS485_DEVICES_NUM];
#endif
	unsigned short log_disable1;
	unsigned short log_disable2;
	uint8_t mqtt1_loglevel;
	uint8_t mqtt2_loglevel;
#ifdef ENABLE_PULSE_COUNTER
	uint8_t pulse_cnt_enabled;
	unsigned short pcnt_threshold; //threshold used to save value in EEPROM
#endif
#ifdef DEMO_BOX
	int16_t temp_min_limit;
	int16_t temp_max_limit;
#endif
	uint8_t tcp_modbus_enabled;
	uint8_t udp_enabled;
#ifdef ENABLE_PT1000
	uint8_t adc_enabled;
#endif
	uint8_t mdns_enabled;
	uint8_t control_task_enabled;
#ifdef EEPROM_STORAGE
	uint8_t energy_log[NUMBER_OF_RECORDERS];
#endif
	uint8_t wifi_ap_enabled;
#ifdef STATIC_IP
	uint8_t static_ip_enabled;
	char ip[16];
	char gateway[16];
	char netmask[16];
	char dns1[16];
	char dns2[16];
#endif
} settingsStruct_t;

//factory settings are programmed in production phase
typedef struct factory_settingsStruct {
	char serial_number[9];
	char model_type[17];
	int8_t hw_ver;
} factory_settingsStruct;

#define NVS_STRING_MAX_LEN (2048)
#define NVS_STRING	(1)
#define NVS_INT8	(2)
#define NVS_INT16	(3)
#define NVS_INT32	(4)
#define NVS_UINT8	(5)
#define NVS_UINT16	(6)

#define NO_RESTART	(0)
#define RESTART		(1)

typedef struct nvs_settings {
	char name[16];
	void *value;  //settings.serial,
	int type;       //NVS_STRING
	const void *default_value;  //settings.serial,
	uint8_t string_size;
	uint8_t reset_required;
} nvs_settings_t;

#define UPGRADE_START		(1)
#define UPGRADE_OK	 		(2)
#define UPGRADE_FAILED 		(3)
#define CERT_FILE_NOT_FOUND	(4)
#define CERT_FILE_TOO_SHORT	(5)
#define CERT_NAME_TOO_SHORT	(6)
#define URL_TOO_SHORT		(7)


typedef struct upgradeStruct {
	int start;
	int end;
	int8_t status;
	int count;
}upgrade_t;

typedef struct detected_devices
{
	char model[17];
	char serial[9];
}detected_devices_t;

typedef struct modbusDevice {
	int8_t uart_port;
	int8_t address;
	int8_t push_link;
	uint8_t index; //index in settings
	int push_interval;
	long next_push_time;
} modbusDevice_t;

typedef struct bicomDevice {
	int8_t uart_port;
	int8_t address;
	int8_t status;
	uint8_t index; //index in settings
	int8_t push_link;
	int push_interval;
	long next_push_time;
} bicomDevice_t;

typedef struct mqttStatus {
	uint8_t enabled;
	uint8_t state;
} mqttStatus_t;

typedef struct statusStruct {
	char ip_addr[16];
	char mac_addr[6];
	char local_time[20]; //11.04.2019 13:46
	//char idf_ver[20];
	char fs_ver[10];
	//char partition[10];
	char app_status[20];
	int connection;
	int uptime;
	int wifi_rssi;
	int errors;
	uint32_t error_flags;
	upgrade_t upgrade;
	long timestamp;
	int cpu_usage;
	detected_devices_t detected_devices[RS485_DEVICES_NUM + 2];
	modbusDevice_t modbus_device[RS485_DEVICES_NUM + 2]; //RS485 devices + 2x IR (measurements, push)
	int8_t measurement_device_count;
	bicomDevice_t bicom_device[RS485_DEVICES_NUM + 1]; //RS485 devices + IR bicom
	int8_t bicom_device_count;
	int8_t left_ir_busy;
	int8_t ir_counter_status;
	unsigned short settings_crc;
	unsigned short nvs_settings_crc;
#ifdef ENABLE_RIGHT_IR_PORT
	int8_t bicom_state;
	uint8_t rs485_used;
#endif
#ifdef ENABLE_PT1000
	int pt1000_temp;
#endif
#ifdef ENABLE_PULSE_COUNTER
	uint32_t dig_in_count;
#endif
#ifdef START_MQTT_TASK
	mqttStatus_t mqtt[2];
#endif
#ifdef EEPROM_STORAGE
	uint32_t power_up_counter;
#endif
	uint8_t wifi_AP_started;
} statusStruct_t;

//led modes
#define LED_OFF (1)
#define LED_ON  (0)
#define LED_BLINK_SLOW	(2)
#define LED_BLINK_FAST	(3)
#define LED_BLINK_SLOW_PERIOD (1300000)
#define LED_BLINK_FAST_PERIOD (100000)

typedef struct led {
	uint8_t port;
	uint8_t mode; //0 = OFF, 1 = ON, 2,3 = blink
	uint8_t state; //0 = OFF, 1 = ON
	//uint32_t timestamp; //timestamp for led toggle
} led_t;

#define UART_2_CTS	UART_PIN_NO_CHANGE

//RS485
#define RS485_RX_BUF_SIZE	(1024*6) //6k for terminal mode
#define RS485_QUEUE			(uart0_queue)

//Right IR
#ifdef ENABLE_RIGHT_IR_PORT
#define RIGHT_IR_BAUD_RATE		(19200)
#define RIGHT_IR_RX_BUF_SIZE	(256)
#define RIGHT_IR_QUEUE			(uart1_queue)
#endif

//Left IR
#ifdef ENABLE_LEFT_IR_PORT
#define LEFT_IR_BAUD_RATE	(19200)
#define LEFT_IR_RX_BUF_SIZE	(1024*6) //6k for terminal mode
#define LEFT_IR_QUEUE		(uart2_queue)
#endif

// Read packet timeout
#define PACKET_READ_TICS        (50 / portTICK_RATE_MS)

#define RS485_DATA_READY_BIT	(1)
#define LEFT_IR_DATA_READY_BIT	(2)
#define RIGHT_IR_DATA_READY_BIT	(4)

#define RS485_PORT_INDEX	(0)
#define RIGHT_IR_PORT_INDEX	(1)
#define LEFT_IR_PORT_INDEX	(2)

//External Watchdog
#define GPIO_FA_RESET		(GPIO_NUM_4) //factory reset pin
#define GPIO_WATCHDOG_PIN	(GPIO_NUM_12) //gpio12

#define DEBUG_CONSOLE_UNDEFINED (0)
#define DEBUG_CONSOLE_INTERNAL	(1)
#define DEBUG_CONSOLE_485		(2)

typedef struct uart_params {
	uint8_t port; //UART_NUM_0, UART_NUM_1, UART_NUM_2
	uint8_t data_ready_bit; //RS485_DATA_READY_BIT
	uint16_t buf_size; //RS485_RX_BUF_SIZE
} uart_params_t;

typedef struct tcp_client_params {
	int device; //3 devices
	char *ip_addr;
	int port;
	//char *data_ptr;
	//int data_len;
	int link; //2 push links
} tcp_client_params_t;

typedef struct
{
  uint8_t power;
  uint8_t phase;
  uint8_t quadrant;
  uint8_t absolute;
  uint8_t invert;
  int8_t  exponent;
  uint8_t tariff;
}counter_config_t;

typedef struct mqtt_topic_struct {
	char publish[65];
	char subscribe[65];
	//char info[100];
	//char stats[70];
} mqtt_topic_t;

typedef struct statisticsStruct {
  int errors;
  int http_requests;
  int mqtt_publish;
  int push;
  int push_errors;
  int push_ack;
  int modbus_slave;
  int tcp_rx_packets;
  int tcp_tx_packets;
  int rs485_rx_packets;
  int rs485_tx_packets;
  int left_ir_rx_packets;
  int left_ir_tx_packets;
  int right_ir_rx_packets;
  int right_ir_tx_packets;
  int eeprom_errors;
} statisticsStruct_t;

#define TCP_SEMAPHORE_TIMEOUT (1000 / portTICK_PERIOD_MS)
#define MODBUS_RTU 0
#define MODBUS_TCP 1

//#define SERIAL_CIRCULAR_BUFFER
#ifdef SERIAL_CIRCULAR_BUFFER
#define SER_BUFF_SIZE (2048)
extern uint8_t serBuff[SER_BUFF_SIZE][3];
extern int serBuffWrPnt[3], serBuffRdPnt[3];
#endif

#define ENERGY_COUNTERS_NUMBER (4)
typedef struct energyCounterValuesStruct {
	uint32_t value[ENERGY_COUNTERS_NUMBER];
} energyCounterValuesStruct_t;

extern settingsStruct_t settings;
extern factory_settingsStruct factory_settings;
extern statusStruct_t status;
extern mqtt_topic_t mqtt_topic[2];
extern counter_config_t counterConfig[20];
extern statisticsStruct_t statistics;
extern int8_t debug_console;
extern int8_t terminal_mode;
extern SemaphoreHandle_t tcp_semaphore;

int mqtt_publish(int mqtt_index, const char *topic, const char *subtopic, const char *data, int len, int qos, int retain);
int read_nvs_setting_i32(char *name, int32_t *value);
int write_nvs_setting_i32(char *name, int32_t value);
int save_settings_nvs(int restart);
int read_settings_nvs();
void printUptime(int uptime, char *time_str);
void print_local_time(int timestamp, char *res);
int get_wifi_rssi();
int bluetooth_init();
int bluetooth_deinit();
void led_set(int led, int state);
int read_file(char *filename, char *out, int size);
void bicom_toggle();
esp_err_t clear_nvs_settings();
void tcp_client_task(void *pvParameters);
char* mystrcat( char* dest, char* src );
int read_modbus_registers(int USARTx, int address, unsigned int startReg, unsigned int nrReg, uint8_t * data);
uint8_t getModbusValue8(uint8_t *data, int offset);
int16_t getModbusValue16(uint8_t *data, int offset);
int32_t getModbusValue32(uint8_t *data, int offset);
unsigned long convertT4ToLong(unsigned short modbus_val);
void convertModbusT5T6_XML(uint8_t *data, int offset, int required_precision, char *result_str);
void convertModbusT5T6(uint8_t *data, int offset, int digits, int unit, char *result_str);
void convertModbusT7(uint8_t *data, int offset, char *result_str);
unsigned short CRC16(unsigned char *puchMsg, int usDataLen);
int modbus_write(int USARTx, int modbus_address, unsigned int reg, unsigned short val, int timeout, char *serial_number);
int send_receive_modbus_serial(int port, char *data, int len, int tcp_packet);
int getEnergyMeasurementsXML(char *push_buf, int USARTx, int modbus_address, int push_interval);
int set_ir_bicom_state(int state);
int getModelAndSerial(int USARTx, int modbus_address, char *model, uint8_t *serial);
int getDescriptionLocation(int USARTx, int modbus_address, char *description, char *location);
int getDescriptionLocationSmall(int USARTx, int modbus_address, char *description, char *location);
void format_energy_counter(long value, int8_t exponent, char *str_result);
void format_energy_counter_XML(long value, int8_t exponent, char *str_result);
int readWMCountersConfiguration(int USARTx, int modbus_address);
int readIECountersConfiguration(int USARTx, int modbus_address);
int getRamLoggerData(int USARTx, int modbus_address);
char *getMeasurementsJSON(int USARTx, int modbus_address, int number);
char *getEnergyCountersJSON(int port, int modbus_address, int number);
char *getStatisticsJSON();
char *getStatusJSON();
esp_err_t drawGraph_handler(httpd_req_t *req);
void bicom_485_toggle(int device);
int set_bicom_485_device_state(int device, int state);
int set_bicom_485_address_state(uint8_t address, int state);
int get_bicom_485_state(int device);
int get_bicom_485_address_state(int address);
unsigned short SwapW(unsigned short i);
int check_tcp_header(char *tcp_ibuf, int len);
int modbus_slave(char *raw_msg, int raw_size, int tcp_modbus);
int convert_to_rtu(char *raw_msg, int raw_size, int *tcp_modbus);
void reset_device();
void ble_provisioning_manager(void);
esp_err_t save_u8_key_nvs(char *name, uint8_t value);
esp_err_t save_i8_key_nvs(char *name, int8_t value);
esp_err_t read_i8_key_nvs(char *name, int8_t *value);
esp_err_t save_u16_key_nvs(char *name, uint16_t value);
esp_err_t read_u16_key_nvs(char *name, uint16_t *value);
esp_err_t save_i32_key_nvs(char *name, int32_t value);
esp_err_t read_i32_key_nvs(char *name, int32_t *value);
esp_err_t save_string_key_nvs(char *name, char *value);
esp_err_t read_str_key_nvs(char *name, char *value, size_t required_size);
char *get_spiffs_files_json(char *file_ext);
int get_ir_bicom_state();
void set_log_level1(int tag_id, int level);
void set_log_level2(int tag_id, int level);
void send_info_to_left_IR(int time);
int detect_terminal_mode(u8_t *data);
int detect_terminal_mode_end(u8_t *data);
void delay_us(int delay);
int countActiveRS485Devices();
int MQTT_LOG(int level, int number, const char *TAG, const char *fmt, ...);
int checkTagLoglevel(const char *tag);
char *getBicomStatusJSON(int modbus_address, int is_number);
int getIndexFromAddress(int modbus_address);
void reset_pulse_counter();
void clear_all_keys(nvs_handle my_handle);
void CRC_update();
int getNumberFromIndex(int index);
char *getBicomsJSON();

#ifdef ATECC608A_CRYPTO_ENABLED
void atecc608a_sign_verify_test();
void atecc608a_sign_verify_test_full();
int atecc608a_sleep();
#endif //ATECC608A_CRYPTO

#ifdef EEPROM_STORAGE
#include "driver/i2c.h"

//#define USED_ENERGY_COUNTER 	0 //todo make setting 0: disabled, 1-4 number of energy counter

//EEPROM LOCATIONS
#define EE_STATUS_ADDR (0x0)
#define EE_ENERGY_STATUS_ADDR (0x10)
#define EE_ENERGY_DAYS_ADDR (0x100) //384 longs
#define EE_ENERGY_MONTHS_ADDR (0x700)//48 longs
#define EE_ENERGY_YEARS_ADDR (0x7c0) //16 longs
#define EE_RECORDER_SIZE (0x800)

#define EE_ENERGY_DAYS_NUMBER ((EE_ENERGY_MONTHS_ADDR - EE_ENERGY_DAYS_ADDR)/4) //384 days
#define EE_ENERGY_DAYS_PAGES	((EE_ENERGY_DAYS_NUMBER)/4) //96 pages

#define EE_ENERGY_MONTHS_NUMBER ((EE_ENERGY_YEARS_ADDR - EE_ENERGY_MONTHS_ADDR)/4) //48 months
#define EE_ENERGY_MONTHS_PAGES	((EE_ENERGY_MONTHS_NUMBER)/4) //12 pages

#define EE_ENERGY_YEARS_NUMBER ((EE_RECORDER_SIZE - EE_ENERGY_YEARS_ADDR)/4) //16 years
#define EE_ENERGY_YEARS_PAGES	((EE_ENERGY_YEARS_NUMBER)/4) //4 pages

#define DAY_ENERGY		(0)
#define MONTH_ENERGY	(1)
#define YEAR_ENERGY		(2)

#define EE_STATUS_SIZE (16)
typedef struct EEstatusStruct {
	uint32_t power_up_counter;
	uint32_t dig_in_count;
	uint8_t reserved[6];
	uint16_t crc;
} EEstatusStruct_t;

extern EEstatusStruct_t ee_status;

#define EE_ENERGY_STATUS_SIZE (16)
typedef struct EEenergyStruct {
	uint32_t timestamp;
	uint16_t energy_day_wp; //write pointer
	uint8_t energy_month_wp;
	uint8_t energy_year_wp;
	uint8_t reserved[8];
} EEenergyStruct_t;

extern EEenergyStruct_t ee_energy_status[NUMBER_OF_RECORDERS];

// Definitions for i2c
#define EEPROM_PAGE_SIZE	(16)  //todo:24LC64 has 32 byte page
#define EEPROM_I2C_ADDR		(0x50)

#define EE_POWER_UP_COUNTER_LOCATION	(0x00)
//#define EE_PULSE_COUNTER_LOCATION		(0x100)

#define I2C_SEMAPHORE_TIMEOUT		(1000 / portTICK_PERIOD_MS)
#define I2C_SEMAPHORE_TIMEOUT_ERROR	(-2)

typedef struct
{
  uint8_t address;
  uint32_t baud_rate;
  uint8_t parity;
  uint8_t stop_bits;
  char serial_number[10];
  char model[18];
}detected_rs485_t;
extern detected_rs485_t rs485_detected_list[16];

void i2c_init();
esp_err_t i2c_master_driver_initialize(void);
esp_err_t EE_write_status();
esp_err_t EE_write_energy_status();
esp_err_t EE_write_energy_page(int page, uint8_t *values);
uint32_t eeprom_read_long(uint8_t deviceaddress, uint16_t eeaddress);
esp_err_t eeprom_write_long(uint8_t deviceaddress, uint16_t eeaddress, uint32_t data);
esp_err_t eeprom_page_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size);
esp_err_t eeprom_read(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data, size_t size);
uint8_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data);
esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte);
int getEnergyCounterValue(int USARTx, int modbus_address, energyCounterValuesStruct_t *counter_values);
char *EE_read_energy_day_records_JSON(int recorder);
void energy_counter_recorder(struct tm timeinfo);
void erase_energy_recorder(int recorder);
int i2c_lock_mutex();
int i2c_unlock_mutex();
int ee_write_energy_value(int location, uint32_t value, int type, int recorder);
void set_energy_wp(int recorder, int day, int month, int year);
void test_write_energy();
#endif //EEPROM_STORAGE

int getDayFromTime(int timestamp);
int getMonthFromTime(int timestamp);
int getYearFromTime(int timestamp);
void wifi_init();
void start_soft_ap();

char *search_by_serial_number_JSON(int baud_rate, int parity, int stop_bits);
void set_rs485_params(int baud_rate, uint8_t stop_bits);
int writeFile(char *fname, char *buf);
