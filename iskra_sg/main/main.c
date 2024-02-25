/* ethernet Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "include/main.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "esp_bt.h"

static const char *TAG = "main";
const char* upgradeStatusStr[] = { "N/A", "Started", "OK", "Failed", "Cert. File Not Found",
		"Cert. File Too Small", "Cert. FileName Too Short", "Upgrade URL Too Short" };
settingsStruct_t settings;
factory_settingsStruct factory_settings;
statusStruct_t status;
EventGroupHandle_t ethernet_event_group;
EventGroupHandle_t uart_event_group;
statisticsStruct_t statistics;

const esp_app_desc_t *app_desc;
char *push_buf[2]; //push buffer pointers
const char* console_apps[] = {"ADC","ATECC608A","bicom","control_task","DIG_INPUT","energy meter","ethernet_init","graph",
		"http_server","LED","LEFT_IR","main","modbus_client","modbus_slave","MQTT","NTP Client",
		"nvs","ota","push","RS485_APP",	"TCP_client","TCP Server","UDP","utils","wifi_init", "eeprom" }; //26 tags

int8_t debug_console;
int8_t terminal_mode = 0;

static esp_err_t init_spiffs(void);
void wifi_init(void);

#ifdef START_MQTT_TASK
void mqtt_client_task(void *pvParam);
#endif

#ifdef NTP_ENABLED
void ntp_client_task(void *pvParam);
#endif

#ifdef MODBUS_TCP_SERVER
void tcp_server_task(void *pvParam);
#endif

#ifdef UDP_SERVER
void udp_server_task(void *pvParameters);
#endif

#ifdef START_CONTROL_TASK
void start_control_task();
#endif

#ifdef WEB_SERVER_ENABLED
void http_server_init();
#endif

#ifdef FACTORY_RESET_ENABLED
static void factory_reset_pin_init_input();
static void factory_reset_check();
int8_t factory_reset_state;
#endif

#ifdef ENABLE_PT1000
void adc_task(void *pvParameters);
#endif

#ifdef ENABLE_PULSE_COUNTER
void pulse_counter_task(void *pvParam);
#endif

#ifdef ENABLE_ETHERNET
void ethernet_init();
#endif

#ifdef LED_ENABLED
void led_task(void *pvParam);
#endif

#ifdef ENABLE_RS485_PORT
void start_rs485_master_task();
#if CONFIG_BOARD_SG
void start_rs485_slave_task();
#endif
#endif

#ifdef ENABLE_LEFT_IR_PORT
void start_left_ir_task();
#endif

#ifdef ENABLE_RIGHT_IR_PORT
void right_ir_task();
#endif

#if CONFIG_BOARD_SG
#ifdef ENABLE_RS485_PORT
void rs485_tx_enable_pin_init();
#endif
#endif

#ifdef EXTERNAL_WATCHDOG_ENABLED
void start_wdt_task();
#endif

#ifdef EEPROM_STORAGE
EEstatusStruct_t ee_status;
EEenergyStruct_t ee_energy_status[NUMBER_OF_RECORDERS];
uint8_t eeprom_size;
#endif

static void initialise_mdns(void);
static void print_upgrade_status(void);
static int detect_devices();

void app_main()
{
	EventBits_t uxBits;
	int connection_timeout;
	long detection_timestamp = 0; //device detection

	ethernet_event_group = xEventGroupCreate(); //zurb it needs to be present before handler
	uart_event_group = xEventGroupCreate();

#ifdef EXTERNAL_WATCHDOG_ENABLED
	start_wdt_task();
#endif

	//start led task
#ifdef LED_ENABLED
	xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);
	led_set(LED_RED, LED_ON); //indicate application start
	vTaskDelay(200 / portTICK_PERIOD_MS);
	led_set(LED_RED, LED_OFF); //indicate application start
	led_set(LED_GREEN, LED_ON); //indicate application start
	vTaskDelay(200 / portTICK_PERIOD_MS);
#endif //#if LED_ENABLED

	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	MY_LOGI(TAG, "This is %s chip with %d CPU cores, WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
					(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	MY_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

	MY_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	status.connection = STATUS_NO_CONNECTION;

	// Initialize NVS
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		MY_LOGE(TAG, "nvs_flash_init Failed:%d. Erasing NVS", err);
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	memset((void *)&settings, 0, sizeof(settings));

	//read saved crc
	read_u16_key_nvs("settings_crc", &status.nvs_settings_crc);

	int settings_retry = 5;
	while(settings_retry--)
	{
		err = read_settings_nvs();
		if(err != ESP_OK)
		{
			MY_LOGE(TAG, "Read settings from NVS Failed:%d", err);
			status.error_flags |= ERR_NVS_SETTINGS;
		}
		else
		{
			//calculate crc
			status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
			if(status.nvs_settings_crc == 0) //1st time
			{
				MY_LOGW(TAG, "NVS settings CRC: 0");
				save_u16_key_nvs("settings_crc", status.settings_crc);
				status.nvs_settings_crc = status.settings_crc;
			}
			if(status.nvs_settings_crc != status.settings_crc)
			{
				MY_LOGE(TAG, "Settings CRC:%04X NVS:%04X", status.settings_crc, status.nvs_settings_crc);
				status.error_flags |= ERR_SETTINGS_CRC;
			}
			else
			{
				status.error_flags &= ~ERR_SETTINGS_CRC;
				status.error_flags &= ~ERR_NVS_SETTINGS;
				break; //settings OK, exit loop
			}
		}
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}


	if(factory_settings.serial_number[0] == 0 || strlen(factory_settings.serial_number) > 8 || strlen(factory_settings.serial_number) < 8)//check if serial number is OK)
	{
		MY_LOGW(TAG, "Serial number not OK. Using %s from menuconfig", CONFIG_SERIAL_NUMBER);
		strcpy(factory_settings.serial_number, CONFIG_SERIAL_NUMBER);
		save_string_key_nvs("serial_number", factory_settings.serial_number);
		CRC_update();//update CRC
	}

	if(factory_settings.model_type[0] == 0 || strlen(factory_settings.model_type) > 16 || strlen(factory_settings.model_type) < 3)
	{
		MY_LOGW(TAG, "Model type not OK. Using %s", DEFAULT_MODEL_TYPE);
		strcpy(factory_settings.model_type, DEFAULT_MODEL_TYPE);
		save_string_key_nvs("model_type", DEFAULT_MODEL_TYPE);
		CRC_update();//update CRC
	}

	read_i8_key_nvs("hw_ver", &factory_settings.hw_ver);
	MY_LOGI(TAG, "HW version: %d", factory_settings.hw_ver);
	if(factory_settings.hw_ver == 0)//unitialised
	{
		eeprom_size = 0;
		MY_LOGE(TAG, "Unknown EEPROM");
	}
	else if(factory_settings.hw_ver > 1)
	{
		eeprom_size = 64;
		MY_LOGI(TAG, "EEPROM 24LC64");
	}
	else
	{
		eeprom_size = 16;
		MY_LOGI(TAG, "EEPROM 24LC16");
	}

	MY_LOGW(TAG, "Settings size:%d CRC:%04X NVS CRC:%04X", sizeof(settings), status.settings_crc, status.nvs_settings_crc);

#ifdef FACTORY_RESET_ENABLED
	factory_reset_check(); //blocking function (until reset button is released)
#endif

#ifdef ATECC608A_CRYPTO_ENABLED
	//atecc608a_test();
	//atecc608a_sign_verify_test();
	int i;
	for(i = 0; i < 1; i++)
		atecc608a_sign_verify_test();

	atecc608a_sleep();
	//i2c_driver_delete(I2C_NUM_0);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
#endif //ATECC608A_CRYPTO

#ifdef EEPROM_STORAGE
	uint16_t tmp_crc = 0;

	i2c_init();

#if 0 //clear status page
	memset((void*)&ee_status, 0, EE_STATUS_SIZE);
	EE_write_status();
#endif

	if(eeprom_size > 0)
	{

		int retry = 2;
		while(retry --)
		{
			eeprom_read(EEPROM_I2C_ADDR, EE_STATUS_ADDR, (uint8_t*)&ee_status, EE_STATUS_SIZE);
			tmp_crc = CRC16((unsigned char *)&ee_status, EE_STATUS_SIZE - 2);
			if(ee_status.crc != tmp_crc)
			{
				MY_LOGE(TAG, "EE Status CRC:0x%04x read:0x%04x", ee_status.crc, tmp_crc);
				ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_status, EE_STATUS_SIZE, ESP_LOG_INFO);
				vTaskDelay(200/portTICK_PERIOD_MS);
			}
			else
				break;
		}

		if(ee_status.crc != tmp_crc)//eeprom read or crc failed
		{
			status.error_flags |= ERR_EEPROM;
			MY_LOGE(TAG, "EE Status page CRC:0x%04x read:0x%04x", ee_status.crc, tmp_crc);
			ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_status, EE_STATUS_SIZE, ESP_LOG_ERROR);
			//clear eeprom status and write it
			MY_LOGW(TAG, "Clearing EE status_page");
			memset((void*)&ee_status, 0, EE_STATUS_SIZE);
			EE_write_status();
		}

		MY_LOGI(TAG, "EE status");
		ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_status, EE_STATUS_SIZE, ESP_LOG_INFO);
		status.dig_in_count = ee_status.dig_in_count;
		status.power_up_counter = ee_status.power_up_counter; //eeprom_read_long(EEPROM_I2C_ADDR, EE_POWER_UP_COUNTER_LOCATION);
		MY_LOGI(TAG, "Power up counter: %u\n\r", status.power_up_counter);
		MY_LOGI(TAG, "Digital input   : %u\n\r", status.dig_in_count);
		//vTaskDelay(100/portTICK_PERIOD_MS);
		status.power_up_counter++;
		ee_status.power_up_counter = status.power_up_counter;
		EE_write_status();

		//energy status
		MY_LOGI(TAG, "EE energy status");
		for(int i = 0; i < NUMBER_OF_RECORDERS; i++)
		{
			if((i > 0 && eeprom_size == 16))
				break; //only one recorder
			eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_STATUS_ADDR + i*EE_RECORDER_SIZE , (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE);
			ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);
			MY_LOGI(TAG, "Energy timestamp %d: %u", i, ee_energy_status[i].timestamp);
			MY_LOGI(TAG, "Energy day       %d: %u", i, ee_energy_status[i].energy_day_wp);
			if(ee_energy_status[i].energy_day_wp >= EE_ENERGY_DAYS_NUMBER)
				ee_energy_status[i].energy_day_wp = 0;
			MY_LOGI(TAG, "Energy month     %d: %u", i, ee_energy_status[i].energy_month_wp);
			if(ee_energy_status[i].energy_month_wp >= EE_ENERGY_MONTHS_NUMBER)
				ee_energy_status[i].energy_month_wp = 0;
			MY_LOGI(TAG, "Energy year      %d: %u", i, ee_energy_status[i].energy_year_wp);
			if(ee_energy_status[i].energy_year_wp >= EE_ENERGY_YEARS_NUMBER)
				ee_energy_status[i].energy_year_wp = 0;
		}

		//EE_write_energy_status();
	}//if(eeprom_size == 0)
#endif //EEPROM_STORAGE

#if CONFIG_BOARD_SG
#ifdef ENABLE_RS485_PORT
	uint8_t i;
	status.rs485_used = 0;
	read_i8_key_nvs("debug_console", &debug_console);
	if(debug_console == DEBUG_CONSOLE_UNDEFINED)
		debug_console = DEBUG_CONSOLE_485; //default
	MY_LOGI(TAG, "NVS debug_console: %d", debug_console);
	rs485_tx_enable_pin_init();
	for(i = 0; i < RS485_DEVICES_NUM; i++)
	{
		if(settings.rs485[i].type != 0)
		{
			status.rs485_used = 1; //rs485 is used and not available for console
			break;
		}
	}
	if((debug_console == DEBUG_CONSOLE_485) && status.rs485_used == 0)//rs485 disabled, so we can use port for debug or modbus slave
	{
		MY_LOGI(TAG, "debug_console on RS485 port");
		gpio_set_level(GPIO_NUM_2, 1); //enable 485 TX: debug goes to RS485 port (default setting)
	}
	else
	{
		MY_LOGI(TAG, "debug_console on internal pin");
		debug_console = DEBUG_CONSOLE_INTERNAL; //turn off debug console till next reboot
		status.rs485_used = 1; //debug console is on internal pin, so rs485 can be used for external devices
		gpio_set_level(GPIO_NUM_2, 0); //disable 485 TX: debug is on internal pin
	}
#endif //ENABLE_RS485_PORT
#endif

	app_desc = esp_ota_get_app_description();

	//tcpip_adapter_init();
	esp_netif_init();

	MY_LOGI(TAG, "[APP] IDF version  : %s", esp_get_idf_version());
	MY_LOGI(TAG, "[APP] Free memory  : %d bytes", esp_get_free_heap_size());
	MY_LOGI(TAG, "[APP] Version      : %s", app_desc->version);
	MY_LOGI(TAG, "[APP] Serial number: %s", factory_settings.serial_number);
	MY_LOGI(TAG, "[APP] Model Type   : %s", factory_settings.model_type);

	print_upgrade_status();

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	//start wifi or ethernet init.
	status.connection = STATUS_CONNECTING;

	/* MDNS and NetBios */
#ifdef MDNS_ENABLED
	if(settings.mdns_enabled)
	{
		initialise_mdns();
		netbiosns_init();
		netbiosns_set_name(factory_settings.serial_number);
	}
#endif

	//Create the semaphore for serial port buffer
	tcp_semaphore = xSemaphoreCreateMutex();
	//Assume serial port buffer is ready
	xSemaphoreGive(tcp_semaphore);

	/*
	 * Connect
	 */
	//wifi or default connection
	if(settings.connection_mode == ETHERNET_MODE )
	{
		MY_LOGI(TAG, "******************************");
		MY_LOGI(TAG, "*** Connecting to Ethernet ***");
		MY_LOGI(TAG, "******************************");
		esp_wifi_set_mode(WIFI_MODE_NULL); //turn OFF wifi
		ethernet_init();
	}
	else //wifi
	{
		MY_LOGI(TAG, "**************************");
		MY_LOGI(TAG, "*** Connecting to WiFi ***");
		MY_LOGI(TAG, "**************************");
		wifi_init();
	}

	connection_timeout = CONNECT_TIMEOUT * 1000 / portTICK_PERIOD_MS;

#if 0 //try to release some heap but it doesnt work
	err = esp_bt_controller_deinit();
	if (err != ESP_OK)
		MY_LOGE(TAG, "Failed to deinit Bluetooth");
#endif

#ifdef START_CONTROL_TASK
	//start control task
	if(settings.control_task_enabled)
		start_control_task();
#endif

	/* Initialize SPIFFS */
	init_spiffs();

	/* init uarts */
	for(int i = 0; i < RS485_DEVICES_NUM + 2; i++)
		status.modbus_device[i].uart_port = -1; //not available by default

#ifdef ENABLE_LEFT_IR_PORT
	if(settings.left_ir_enabled)
	{
		start_left_ir_task(); //left_ir_task
		//override active uart port and modbus address for measurements, counters... because left IR has highest priority
		status.modbus_device[status.measurement_device_count].uart_port = LEFT_IR_UART_PORT;
		status.modbus_device[status.measurement_device_count].address = settings.ir_counter_addr;
		if(settings.left_ir_push.push_link > 0)
		{
			status.modbus_device[status.measurement_device_count].push_link = settings.left_ir_push.push_link;
			if(settings.left_ir_push.push_interval < 60 || settings.left_ir_push.push_interval > 60*60*12)
				settings.left_ir_push.push_interval = 600;//default 10 minutes,
			status.modbus_device[status.measurement_device_count].push_interval = settings.left_ir_push.push_interval;

			status.modbus_device[status.measurement_device_count].next_push_time =
					status.timestamp + settings.left_ir_push.push_interval -
					((status.timestamp + settings.left_ir_push.push_interval) % settings.left_ir_push.push_interval); //set 1st push time
			MY_LOGI(TAG, "IR counter Publish %d 1st push:%ld", status.measurement_device_count, status.modbus_device[status.measurement_device_count].next_push_time);
		}

		status.measurement_device_count++;

		//send_info_to_left_IR(3600); //send status and IP address to wm3, so we can see IP address on wm3's LCD (valid 3600s)
	}
#endif

#ifdef ENABLE_RIGHT_IR_PORT
	if(settings.ir_ext_rel_mode > 0)
	{
		right_ir_task(); //right_ir_task

		status.bicom_device[status.bicom_device_count].uart_port = RIGHT_IR_UART_PORT;
		status.bicom_device[status.bicom_device_count].address = 0;
		if(settings.right_ir_push.push_link > 0)
		{
			status.bicom_device[status.bicom_device_count].push_link = settings.right_ir_push.push_link;
			if(settings.right_ir_push.push_interval < 60 || settings.right_ir_push.push_interval > 60*60*12)
				settings.right_ir_push.push_interval = 600;//default 10 minutes,
			status.bicom_device[status.bicom_device_count].push_interval = settings.right_ir_push.push_interval;

			status.bicom_device[status.bicom_device_count].next_push_time =
					status.timestamp + settings.right_ir_push.push_interval -
					((status.timestamp + settings.right_ir_push.push_interval) % settings.right_ir_push.push_interval); //set 1st push time
			MY_LOGI(TAG, "IR Bicom Publish %d 1st push:%ld", status.bicom_device_count, status.bicom_device[status.bicom_device_count].next_push_time);
		}
		status.bicom_device_count++;
	}
#endif

#if CONFIG_BOARD_SG
#ifdef ENABLE_RS485_PORT
	if(status.rs485_used == 1)
		start_rs485_master_task();
	else //rs485 is disabled, so we can use port for modbus slave
		start_rs485_slave_task();

	//define active uart port and modbus address for measurements, counters...
	for(i = 0; i < RS485_DEVICES_NUM; i++)
	{
		status.bicom_device[i].status = -1;

		if(settings.rs485[i].type == RS485_ENERGY_METER || settings.rs485[i].type == RS485_PQ_METER)
		{
			status.modbus_device[status.measurement_device_count].uart_port = RS485_UART_PORT;
			status.modbus_device[status.measurement_device_count].address = settings.rs485[i].address;
			status.modbus_device[status.measurement_device_count].index = i;
			if(settings.rs485_push[i].push_link > 0)
			{
				status.modbus_device[status.measurement_device_count].push_link = settings.rs485_push[i].push_link;
				if(settings.rs485_push[i].push_interval < 60 || settings.rs485_push[i].push_interval > 60*60*12)
					settings.rs485_push[i].push_interval = 600;//default 10 minutes,
				status.modbus_device[status.measurement_device_count].push_interval = settings.rs485_push[i].push_interval;

				status.modbus_device[status.measurement_device_count].next_push_time =
						status.timestamp + settings.rs485_push[i].push_interval -
						((status.timestamp + settings.rs485_push[i].push_interval) % settings.rs485_push[i].push_interval); //set 1st push time
				MY_LOGI(TAG, "Publish %d 1st push:%ld", status.measurement_device_count, status.modbus_device[status.measurement_device_count].next_push_time);
			}
			status.measurement_device_count++;
		}
		else if(settings.rs485[i].type == RS485_BISTABILE_SWITCH)
		{
			status.bicom_device[status.bicom_device_count].uart_port = RS485_UART_PORT;
			status.bicom_device[status.bicom_device_count].address = settings.rs485[i].address;
			status.bicom_device[status.bicom_device_count].index = i; //index in settings

			if(settings.rs485_push[i].push_link > 0)
			{
				status.bicom_device[status.bicom_device_count].push_link = settings.rs485_push[i].push_link;
				if(settings.rs485_push[i].push_interval < 60 || settings.rs485_push[i].push_interval > 60*60*12)
					settings.rs485_push[i].push_interval = 600;//default 10 minutes,
				status.bicom_device[status.bicom_device_count].push_interval = settings.rs485_push[i].push_interval;

				status.bicom_device[status.bicom_device_count].next_push_time =
						status.timestamp + settings.rs485_push[i].push_interval -
						((status.timestamp + settings.rs485_push[i].push_interval) % settings.rs485_push[i].push_interval); //set 1st push time
				MY_LOGI(TAG, "Bicom %d 1st push:%ld", status.bicom_device_count + 1, status.bicom_device[status.bicom_device_count].next_push_time);
			}

			status.bicom_device_count++;
		}
	}
	MY_LOGI(TAG, "Bicom number: %d", status.bicom_device_count);

#endif // ENABLE_RS485_PORT
#endif

#if CONFIG_BOARD_IE14MW
	start_rs485_master_task(); //this call only initialise RS485 port
	status.modbus_device[0].uart_port = RS485_UART_PORT;
	status.modbus_device[0].address = 33;
	status.measurement_device_count = 1;
#endif

	if(status.measurement_device_count == 0)
			MY_LOGW(TAG, "No active device for modbus readings");
	else
	{
		MY_LOGI(TAG, "Active devices: %d", status.measurement_device_count);
		for(int i = 0; i < status.measurement_device_count; i++)
			MY_LOGI(TAG, "Active device %d, port:%d modbus address:%d", i+1, status.modbus_device[i].uart_port, status.modbus_device[i].address);
	}

#ifdef ENABLE_PULSE_COUNTER
	//start pulse counter task
	if(settings.pulse_cnt_enabled)
		xTaskCreate(&pulse_counter_task, "pulse_cnt", 3096, NULL, 5, NULL);
#endif

#ifdef ENABLE_PT1000
	//start adc task
	if(settings.adc_enabled)
		xTaskCreate(&adc_task, "adc", 2048, NULL, 5, NULL);
#endif

	status.pt1000_temp = 0xffff; //N/A status

	//led_set(LED_GREEN, LED_BLINK_SLOW); //indicate connection state

	//----- WAIT FOR GOT_IP_BIT -----
	MY_LOGI(TAG, "waiting for GOT_IP_BIT ...");
	uxBits = xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, connection_timeout);
	if( ( uxBits & GOT_IP_BIT ) == GOT_IP_BIT )
	{
		MY_LOGI(TAG, "GOT_IP_BIT!");
	}
	else
	{
		led_set(LED_RED, LED_ON); //indicate not connected state
		MY_LOGE(TAG, "CONNECT TIMEOUT");
		reset_device();
	}

#ifdef START_MQTT_TASK
	//start mqtt_client
	if(settings.publish_server[0].push_protocol == MQTT_PROTOCOL ||
			settings.publish_server[1].push_protocol == MQTT_PROTOCOL)
		xTaskCreate(&mqtt_client_task, "mqtt_client", 4096, NULL, 5, NULL);
#endif

#ifdef NTP_ENABLED
	//start ntp task
	if(settings.time_sync_src == 1)
		xTaskCreate(&ntp_client_task, "ntp_client", 1024*3, NULL, 5, NULL);
#endif //NTP_ENABLED

#ifdef MODBUS_TCP_SERVER
	//tcp server
	if(settings.tcp_modbus_enabled)
		xTaskCreate(&tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
#endif

#ifdef UDP_SERVER
	//udp server
	if(settings.udp_enabled)
		xTaskCreate(&udp_server_task, "udp_server", 4096, NULL, 5, NULL);
#endif

	//web server
#ifdef WEB_SERVER_ENABLED
	strcpy(status.app_status, "Normal");
	http_server_init();
#endif

	status.bicom_state = -1; //default is not available

#if 0 //#ifdef ENABLE_RIGHT_IR_PORT
	//2 bicom toggles at start
	if(settings.ir_ext_rel_mode > 0)
		bicom_toggle();
	vTaskDelay(1000 / portTICK_PERIOD_MS);
#endif //ENABLE_RIGHT_IR_PORT

	if(settings.publish_server[0].push_resp_time < 5)
		settings.publish_server[0].push_resp_time = 5;
	if(settings.publish_server[1].push_resp_time < 5)
		settings.publish_server[1].push_resp_time = 5;

	//set_log_level1(ADC_DEBUG | DIG_INPUT_DEBUG | CONTROL_DEBUG, 2); //disable adc and digital input logging
	set_log_level1(settings.log_disable1, ESP_LOG_WARN);
	set_log_level2(settings.log_disable2, ESP_LOG_WARN);

/*
 *  main 1 second loop
 */

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
	int rollback_flag = 1;
#endif

	while(1)
	{

#ifdef FACTORY_RESET_ENABLED
		if(factory_reset_state)// if factory reset state is set, wait 10 seconds and then turn it off
		{
			factory_reset_state++;
			if(factory_reset_state > 10)
			{
				factory_reset_state = 0;
				MY_LOGW(TAG, "Factory reset back to idle (10 seconds passed)");
				save_i8_key_nvs(FACTORY_RESET_NVS_KEY, 0); //factory reset to idle
			}
		}
#endif

		//update time
		time_t now;
		struct tm timeinfo;

		time(&now);
		status.timestamp = now;
		localtime_r(&now, &timeinfo);
		strftime(status.local_time, sizeof(status.local_time),"%d.%m.%Y %H:%M:%S", &timeinfo);
		//MY_LOGI(TAG, "The current date/time is: %s", status.local_time);

		status.uptime = esp_timer_get_time()/1000000;

		//detect connected devices
		if(now > detection_timestamp)
		{
			detection_timestamp = now + 300; //every 5 minutes
			int ret = detect_devices();
			if(ret)
				detection_timestamp = now + 60; //retry in 60 seconds if detection fails
		}

		//every minute
		if((esp_timer_get_time()/1000000)%60 == 0)
		{
#ifdef DISPLAY_STACK_FREE
			UBaseType_t uxHighWaterMark;

			/* Inspect our own high water mark on entering the task. */
			uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

			MY_LOGI(TAG, "STACK: %d, Heap free size: %d Heap min: %d",
					uxHighWaterMark, xPortGetFreeHeapSize(), esp_get_minimum_free_heap_size());
#endif
			if(settings.connection_mode != ETHERNET_MODE)
				status.wifi_rssi = get_wifi_rssi();

#ifdef EEPROM_STORAGE
			if(eeprom_size > 0)
			{
				if(ee_status.dig_in_count != status.dig_in_count)//save counter if there is a change
				{
					ee_status.dig_in_count = status.dig_in_count;//update digital input counter
					EE_write_status();
				}
				//set_energy_wp(0, 27, 2, 1);//test
				//test_write_energy();//todo test
				energy_counter_recorder(timeinfo);
			}
#endif
			//check settings CRC
			status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
			if(status.nvs_settings_crc != status.settings_crc)
			{
				MY_LOGE(TAG, "Settings CRC:%04X NVS:%04X", status.settings_crc, status.nvs_settings_crc);
				status.error_flags |= ERR_SETTINGS_CRC;
			}
			else
				status.error_flags &= ~ERR_SETTINGS_CRC;

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
			//mark application as valid and disable rollback
			if(rollback_flag)
			{
				MY_LOGI(TAG, "APP is OK. Disable rollback");
				esp_ota_mark_app_valid_cancel_rollback();
				rollback_flag = 0;
			}
#endif
		}

		//push/publish measurements
		for(uint8_t i = 0; i < status.measurement_device_count; i++) //all modbus devices
		{
			int push_bits = status.modbus_device[i].push_link;

			if(push_bits > 0 && status.uptime > 60 && status.modbus_device[i].next_push_time <= status.timestamp && strcmp(status.app_status, "Upgrading"))
			{
				for(int link = 0; link < 2; link++) //every device has 2 links
				{
					if(push_bits & (1<<link)) //push to link enabled?
					{
						//MY_LOGI(TAG, "Push device:%d link:%d protocol:%d", i+1, link, settings.publish_server[link].push_protocol);
						//MY_LOGI(TAG, "Push time:%ld", status.timestamp);
						status.modbus_device[i].next_push_time = status.timestamp + status.modbus_device[i].push_interval -
								((status.timestamp + status.modbus_device[i].push_interval) % status.modbus_device[i].push_interval); //round
						//MY_LOGI(TAG, "Next push:%ld", status.modbus_device[i].next_push_time);

#ifdef START_MQTT_TASK
						if(settings.publish_server[link].push_protocol == MQTT_PROTOCOL)
						{
							char *msg = getMeasurementsJSON(status.modbus_device[i].uart_port, status.modbus_device[i].address, i);
							if(msg == NULL)
							{
								char err_string[70];
								MY_LOGE(TAG, "mqtt device:%d getMeasurementsJSON failed", i+1);
								if(status.modbus_device[i].uart_port == LEFT_IR_UART_PORT)
									sprintf(err_string, "Error: Left IR Energy meter not responding");
								else
									sprintf(err_string, "Error: RS485 device:%d Energy meter not responding. Addr:%d",
											status.modbus_device[i].index + 1, status.modbus_device[i].address);
								mqtt_publish(link, mqtt_topic[link].publish, "error", err_string, 0, settings.publish_server[link].mqtt_qos, 0);

								statistics.errors++;
							}
							else
							{
								char subtopic[25];
								if(status.modbus_device[i].uart_port == LEFT_IR_UART_PORT)
									sprintf(subtopic, "measurements/ir");
								else
									sprintf(subtopic, "measurements/rs485/%d", status.modbus_device[i].index + 1);
								statistics.mqtt_publish++;
								//MY_LOGI(TAG, "PUB size:%d %s", strlen(msg), msg);
								mqtt_publish(link, mqtt_topic[link].publish, subtopic, msg, 0, settings.publish_server[link].mqtt_qos, 0);
								free(msg);
							}

							char *msg1 = getEnergyCountersJSON(status.modbus_device[i].uart_port, status.modbus_device[i].address, i);
							if(msg == NULL)
							{
								char err_string[70];
								MY_LOGE(TAG, "mqtt device:%d getEnergyCountersJSON failed", i+1);
								if(status.modbus_device[i].uart_port == LEFT_IR_UART_PORT)
									sprintf(err_string, "Error: Left IR Energy counter not responding");
								else
									sprintf(err_string, "Error: RS485 device:%d Energy counter not responding. Addr:%d",
											status.modbus_device[i].index + 1, status.modbus_device[i].address);
								mqtt_publish(link, mqtt_topic[link].publish, "error", err_string, 0, settings.publish_server[link].mqtt_qos, 0);
								statistics.errors++;
							}
							else
							{
								char subtopic[25];
								if(status.modbus_device[i].uart_port == LEFT_IR_UART_PORT)
									sprintf(subtopic, "counters/ir");
								else
									sprintf(subtopic, "counters/rs485/%d", status.modbus_device[i].index + 1);
								statistics.mqtt_publish++;
								mqtt_publish(link, mqtt_topic[link].publish, subtopic, msg1, 0, settings.publish_server[link].mqtt_qos, 0);
								free(msg1);
							}
						}
#endif
#ifdef TCP_CLIENT_ENABLED
						if(settings.publish_server[link].push_protocol == MISMART_PUSH)
						{
							tcp_client_params_t tcp_params = {
									.device = i,
									.ip_addr = settings.publish_server[link].mqtt_server,
									.port = settings.publish_server[link].mqtt_port,
									.link = link,
									//.data_len = 10,
							};
							xTaskCreate(&tcp_client_task, "tcp_client", 4096, &tcp_params, 5, NULL);
							//
						}//MISMART_PUSH
#endif //TCP_CLIENT_ENABLED
					}
				}
			}//push time
		}//for all modbus devices

		//push/publish bicoms
		for(uint8_t i = 0; i < (status.bicom_device_count); i++) //all bicoms
		{
			int push_bits = status.bicom_device[i].push_link;

			if(push_bits > 0 && status.uptime > 60 && status.bicom_device[i].next_push_time <= status.timestamp && strcmp(status.app_status, "Upgrading"))
			{
				for(int link = 0; link < 2; link++) //every device has 2 links
				{
					if(push_bits & (1<<link)) //push to link enabled?
					{
						//MY_LOGI(TAG, "Bicom Push device:%d link:%d protocol:%d", i+1, link, settings.publish_server[link].push_protocol);
						//MY_LOGI(TAG, "Bicom Push time:%ld", status.timestamp);
						status.bicom_device[i].next_push_time = status.timestamp + status.bicom_device[i].push_interval -
								((status.timestamp + status.bicom_device[i].push_interval) % status.bicom_device[i].push_interval); //round
						//MY_LOGI(TAG, "Bicom Next push:%ld", status.bicom_device[i].next_push_time);

#ifdef START_MQTT_TASK
						if(settings.publish_server[link].push_protocol == MQTT_PROTOCOL)
						{
							char *msg;
							if(status.bicom_device[i].uart_port == RIGHT_IR_UART_PORT)
								msg = getBicomStatusJSON(status.bicom_device[i].address, 0);//send 0 as modbus address
							else
								msg = getBicomStatusJSON(status.bicom_device[i].index, 1);//send index from settings
							if(msg == NULL)
							{
								char err_string[60];
								MY_LOGE(TAG, "mqtt bicom device:%d getBicomStatusJSON failed", i+1);
								if(status.bicom_device[i].uart_port == RIGHT_IR_UART_PORT)
									sprintf(err_string, "Error: Right IR bicom not responding");
								else
									sprintf(err_string, "Error: RS485 :%d bicom not responding. Addr:%d",
											status.bicom_device[i].index + 1, status.bicom_device[i].address);
								mqtt_publish(link, mqtt_topic[link].publish, "error", err_string, 0, settings.publish_server[link].mqtt_qos, 0);

								statistics.errors++;
							}
							else
							{
								char subtopic[20];
								if(status.bicom_device[i].uart_port == RIGHT_IR_UART_PORT)//IR bicom
									sprintf(subtopic, "bicom/ir");
								else
									sprintf(subtopic, "bicom/rs485/%d", status.bicom_device[i].index + 1);
								statistics.mqtt_publish++;
								mqtt_publish(link, mqtt_topic[link].publish, subtopic, msg, 0, settings.publish_server[link].mqtt_qos, 0);
								free(msg);
							}
						}
#endif
					}
				}
			}
		}//bicoms

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

/* reset task for asynch reset*/
/* input param timeout in seconds */
static void reset_task(void *pvParam)
{
	//int *timeout = (int *)pvParam;

	//MY_LOGI(TAG, "Started reset task, timeout:%d", *timeout);

//	vTaskDelay(*timeout * 1000 / portTICK_PERIOD_MS);

	MY_LOGI(TAG, "***********************************\n\
		      		    		Restarting ...\n\
		      		    		***********************************");

	vTaskDelay(1000 / portTICK_PERIOD_MS);
	esp_restart();

	vTaskDelete( NULL );
}

//void reset_device(int timeout)
void reset_device()
{
#ifdef EEPROM_STORAGE
	if((ee_status.dig_in_count != status.dig_in_count) && eeprom_size > 0)//save counter if there is a change
	{
		ee_status.dig_in_count = status.dig_in_count;//update digital input counter
		EE_write_status();
	}
#endif

	xTaskCreate(&reset_task, "reset_task", 2048, NULL, 5, NULL);
	//xTaskCreate(&reset_task, "reset_task", 2048, &timeout, 5, NULL);
}

/* Function to initialize SPIFFS */
static esp_err_t init_spiffs(void)
{
	MY_LOGI(TAG, "Initializing SPIFFS");

	strcpy(status.fs_ver, "ERR");

	esp_vfs_spiffs_conf_t conf = {
			.base_path = "/spiffs",
			.partition_label = NULL,
			.max_files = 10,   // This decides the maximum number of files that can be created on the storage
			.format_if_mount_failed = true
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			MY_LOGE(TAG, "Failed to mount or format filesystem");
			strcat(status.fs_ver, "1");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			MY_LOGE(TAG, "Failed to find SPIFFS partition");
			strcat(status.fs_ver, "2");
		} else {
			MY_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
			strcat(status.fs_ver, "3");
		}
		return ESP_FAIL;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		MY_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
		strcat(status.fs_ver, "4");
		return ESP_FAIL;
	}
	MY_LOGI(TAG, "SPIFFS Partition size: total: %d, used: %d (%d%%)", total, used, (used*100)/total);

	// Open version file for reading
	MY_LOGI(TAG, "Reading version file");
	FILE *f = fopen("/spiffs/version.txt", "r");
	if (f == NULL) {
		MY_LOGE(TAG, "Failed to open file for reading");
		strcat(status.fs_ver, "6");
		return ESP_FAIL;
	}

	char line[64];
	fgets(line, sizeof(line), f);
	strncpy(status.fs_ver, line, 10);
	fclose(f);

	MY_LOGI(TAG, "Filesystem version: %s", line);

	return ESP_OK;
}

#ifdef FACTORY_RESET_ENABLED
#define GPIO_FACTORY_RESET_PIN  (1ULL<<(GPIO_FA_RESET))
static void factory_reset_pin_init_input()
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_INPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_FACTORY_RESET_PIN;
	//disable pull-down mode
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	//disable pull-up mode
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
}

static void factory_reset_check()
{
	MY_LOGI(TAG, "factory_reset started");

	read_i8_key_nvs(FACTORY_RESET_NVS_KEY, &factory_reset_state);
	MY_LOGW(TAG, "Factory reset state: %s", factory_reset_state?"1st step":"Idle");
	factory_reset_pin_init_input(); //set pin as input
	int factory_reset_pin = gpio_get_level(GPIO_FA_RESET);
	MY_LOGI(TAG, "Factory reset pin:%d", factory_reset_pin);
	if(factory_reset_pin == 0) //if reset is still pressed
	{
		int timeout = FACTORY_RESET_TIMEOUT * 10*5; //in seconds
		//vTaskDelay(1000 / portTICK_PERIOD_MS); //wait 1 second so led is not blinking too soon
		if(factory_reset_state == 0)
			led_set(LED_ORANGE, LED_BLINK_SLOW); //indicate factory reset start state 0
		else
			led_set(LED_RED, LED_BLINK_SLOW); //indicate factory reset start state 1

		while(timeout--)
		{
			factory_reset_pin = gpio_get_level(GPIO_FA_RESET);
			if(factory_reset_pin == 1)//reset is not pushed anymore
			{
				if(factory_reset_state)//if 1st step, back to idle
				{
					MY_LOGW(TAG, "Factory reset back to idle");
					factory_reset_state = 0;
					save_i8_key_nvs(FACTORY_RESET_NVS_KEY, 0); //factory reset to idle
				}
				break; //exit from factory reset loop
			}

			vTaskDelay(20 / portTICK_PERIOD_MS);
		}
		if(timeout < 1)
		{
			if(factory_reset_state == 0)//idle state, just set flag for next factory reset procedure
			{
				led_set(LED_GREEN, LED_ON); //indicate factory reset finished
				MY_LOGW(TAG, "Factory reset 1st step");
				factory_reset_state = 1; //set flag, it will be removed after 10 seconds if reset is not pressed again
				save_i8_key_nvs(FACTORY_RESET_NVS_KEY, 1);
			}
			else
			{
				led_set(LED_RED, LED_ON); //indicate factory reset finished
				MY_LOGW(TAG, "Factory reset!!!!!!!!!!!!");
				clear_nvs_settings();
#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
				save_i8_key_nvs(BLE_PROV_NVS_KEY, 0);
#endif
				save_i8_key_nvs(FACTORY_RESET_NVS_KEY, 0); //factory reset to idle
				reset_device();
			}
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		}
	}
	xEventGroupSetBits(ethernet_event_group, FACTORY_RESET_END);
	MY_LOGI(TAG, "Factory reset procedure finished");
	//vTaskDelete( NULL );
}
#endif // FACTORY_RESET_ENABLED

static void initialise_mdns(void)
{
#define MDNS_INSTANCE "Iskra-SG"

    mdns_init();
    mdns_hostname_set(factory_settings.serial_number);
    mdns_instance_name_set(MDNS_INSTANCE);

    char strPort[6];
    sprintf(strPort,"%d", settings.http_port);

    mdns_txt_item_t serviceTxtData[] = {
        {"Type", "Smart Gateway"},
		{"Vendor", "Iskra"},
		{"Port", strPort},
        {"SW Version", app_desc->version}
    };

    MY_LOGI(TAG, "MDNS Serial:%s, port:%d", factory_settings.serial_number, settings.http_port);
    ESP_ERROR_CHECK(mdns_service_add(factory_settings.serial_number, "_http", "_tcp", settings.http_port, serviceTxtData,
    		sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

static void print_upgrade_status()
{
	char res[32];

	read_i32_key_nvs("upgrade_count", &status.upgrade.count);
	MY_LOGI(TAG, "------ Upgrade status ------");
	MY_LOGI(TAG, "Total Upgrade count : %d",status.upgrade.count);
	if(status.upgrade.count > 0)
	{
		read_i32_key_nvs("upgrade_start", &status.upgrade.start);
		print_local_time(status.upgrade.start, res);
		MY_LOGI(TAG, "Last upgrade start  : %s", res);

		read_i32_key_nvs("upgrade_end", &status.upgrade.end);
		if(status.upgrade.end > 0)
		{
			print_local_time(status.upgrade.end, res);
			MY_LOGI(TAG, "Last upgrade end    : %s", res);
		}
		else
			MY_LOGI(TAG, "Last upgrade end    : Not Finished");

		read_i8_key_nvs("upgrade_status", &status.upgrade.status);
		MY_LOGI(TAG, "Last upgrade status : %s",upgradeStatusStr[status.upgrade.status]);
	}
	MY_LOGI(TAG, "----------------------------");
}

static int detect_devices()
{
	int ret;
	int failed = 0;

#ifdef ENABLE_LEFT_IR_PORT
	if(settings.left_ir_enabled)
	{
		memset(status.detected_devices[0].model, 0, 17);
		memset(status.detected_devices[0].serial, 0, 9);
		ret = getModelAndSerial(LEFT_IR_UART_PORT, settings.ir_counter_addr, status.detected_devices[0].model, (uint8_t *)status.detected_devices[0].serial);
		if(ret < 0)
		{
			MY_LOGW(TAG, "Left IR getModelAndSerial failed");
			strcpy(status.detected_devices[0].model, "Not Detected");
			status.ir_counter_status = 250;
			failed++;
		}
		else
		{
			MY_LOGI(TAG, "Left IR : %8s %s %2d", status.detected_devices[0].model, status.detected_devices[0].serial, settings.ir_counter_addr);
			status.ir_counter_status = 3; //connected
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	else
	{
		strcpy(status.detected_devices[0].model, "Disabled");
		status.ir_counter_status = -1;
	}
#endif

#ifdef ENABLE_RIGHT_IR_PORT
	if(settings.ir_ext_rel_mode > 0)
	{
		memset(status.detected_devices[1].model, 0, 17);
		memset(status.detected_devices[1].serial, 0, 9);

		if(get_ir_bicom_state() < 0)
		{
			MY_LOGW(TAG, "Bicom detection failed");
			strcpy(status.detected_devices[1].model, "Not Detected");
			failed++;
		}
		else
		{
			strcpy(status.detected_devices[1].model, "IR Bicom");
			MY_LOGI(TAG, "Right IR: %8s %s N/A", status.detected_devices[1].model, "N/A");
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	else
		strcpy(status.detected_devices[1].model, "Disabled");
#endif //ENABLE_RIGHT_IR_PORT

#ifdef ENABLE_RS485_PORT
	uint8_t i;
	for(i = 0; i < RS485_DEVICES_NUM; i++)
	{
		if(settings.rs485[i].type != 0)
		{
			memset(status.detected_devices[2 + i].model, 0, 17);
			memset(status.detected_devices[2 + i].serial, 0, 9);
			ret = getModelAndSerial(RS485_UART_PORT, settings.rs485[i].address, status.detected_devices[2 + i].model, (uint8_t *)status.detected_devices[2 + i].serial);
			if(ret < 0)
			{
				MY_LOGW(TAG, "RS485 Device %d getModelAndSerial failed", i);
				strcpy(status.detected_devices[2 + i].model, "Not Detected");
				failed++;
			}
			else
			{
				if(settings.rs485[i].type == RS485_BISTABILE_SWITCH)
					status.bicom_device[i].status = get_bicom_485_state(i);
				MY_LOGI(TAG, "RS485 %d : %8s %s %2d", i, status.detected_devices[2 + i].model, status.detected_devices[2 + i].serial, settings.rs485[i].address);
			}

			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
		else
			strcpy(status.detected_devices[2 + i].model, "Disabled");

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
#endif //#ifdef ENABLE_RS485_PORT

	return failed;
}
