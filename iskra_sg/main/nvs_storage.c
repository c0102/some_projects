#include "main.h"

static const char * TAG = "nvs";
static void terminate_string(char *str, int len);
static int save_active_settings();

extern uint8_t eeprom_size;
/*
 * DEFAULT SETTINGS
 */
const settingsStruct_t default_settings = {
		0, //unlocked
		"Description", //description
		"Location", //location
		CONFIG_WIFI_SSID, //menuconfig wifi_ssid
		CONFIG_WIFI_PASSWORD, //menuconfig wifi_password
		CONFIG_NTP_SERVER_1, //ntp_server1
		CONFIG_NTP_SERVER_2, //ntp_server2
		CONFIG_NTP_SERVER_3, //ntp_server3
		{//push/publish
				{PUSH_DISABLED, 10, "", 1883, "", "", "", "", CONFIG_MQTT_SUBSCRIBE_TOPIC, CONFIG_MQTT_PUBLISH_TOPIC, "", "", 0, 0},
				{PUSH_DISABLED, 10, "", 1883, "", "", "", "", CONFIG_MQTT_SUBSCRIBE_TOPIC, CONFIG_MQTT_PUBLISH_TOPIC, "", "", 0, 0}
		},
		0,	//default connection_mode
		CONFIG_WIFI_MAXIMUM_RETRY, //wifi_max_retry
		SG_MODBUS_ADDRESS, //sg_modbus_addr
		SG_MODBUS_TCP_PORT, //tcp_port
		80, //http port
		//1, //debug_console ON
		60, //timezone UTC+1
		0, //time_sync_src
#ifdef ENABLE_LEFT_IR_PORT
		0,	//left_ir_enabled
		33, //ir_counter_addr
		{0, 600}, //left ir push
		"IR meter", //left_ir_desc
#endif
#ifdef ENABLE_RIGHT_IR_PORT
		0, //ir_ext_rel_mode
		"IR Bicom", //ir_relay_description
		{0, 600}, //right ir push
#endif
#ifdef ENABLE_RS485_PORT
		7, //baud rate:115200 40422
		0, //stop_bits:0 40423
		0, //parity_none 40424
		0, //data_bits:8 40425
		{		{0, 1, "RS485 Device 1"}, {0, 2, "RS485 Device 2"}, {0, 3, "RS485 Device 3"}, {0, 4, "RS485 Device 4"},
				{0, 5, "RS485 Device 5"}, {0, 6, "RS485 Device 6"}, {0, 7, "RS485 Device 7"}, {0, 8, "RS485 Device 8"},
				{0, 9, "RS485 Device 9"}, {0, 10, "RS485 Device 10"}, {0, 11, "RS485 Device 11"}, {0, 12, "RS485 Device 12"},
				{0, 13, "RS485 Device 13"}, {0, 14, "RS485 Device 14"}, {0, 15, "RS485 Device 15"}, {0, 16, "RS485 Device 16"}
		}, //RS 485 devices
		{{0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}, {0, 600}}, //rs485 push
#endif
		0xffff, //log_disable 1
		0xffff, //log_disable 2
		0, //mqtt1_loglevel
		0, //mqtt2_loglevel
#ifdef ENABLE_PULSE_COUNTER
		1, //pulse_cnt_enabled
		10, //pcnt_threshold
#endif
#ifdef DEMO_BOX
		250,//temp_min_limit
		350,//temp_max_limit
#endif
		1, //tcp_modbus_enabled
		1, //udp_enabled
#ifdef ENABLE_PT1000
		1, //adc_enabled
#endif
		1, //mdns enabled
		0, //control_task_enabled
#ifdef EEPROM_STORAGE
		{0, 0, 0, 0}, //energy_recorders
#endif
		0, //wifi_ap_enabled
#ifdef STATIC_IP
		0, //DHCP
		"10.96.2.123", //ip
		"10.96.0.1", //gateway
		"255.255.255.0", //netmask
		"8.8.8.8", //dns1
		"8.8.4.4", //dns2
#endif
};

//nvs settings array.
//Be careful, name is limited to 15 characters.
nvs_settings_t nvs_settings_array[] =
{
		/* name				value								type		default_value */
		{"locked",			(void *) &settings.locked,			NVS_INT8,	(void *) &default_settings.locked, 0, NO_RESTART},
		{"description",		(void *) &settings.description,		NVS_STRING,	(void *) &default_settings.description, 40, NO_RESTART},
		{"location",		(void *) &settings.location,		NVS_STRING,	(void *) &default_settings.location, 40, NO_RESTART},
		{"wifi_ssid",		(void *) &settings.wifi_ssid,		NVS_STRING,	(void *) &default_settings.wifi_ssid, 20, RESTART},
		{"wifi_password",	(void *) &settings.wifi_password,	NVS_STRING, (void *) &default_settings.wifi_password, 20, RESTART},
		{"ntp_server1",		(void *) &settings.ntp_server1,		NVS_STRING, (void *) &default_settings.ntp_server1, 40, RESTART},
		{"ntp_server2",		(void *) &settings.ntp_server2,		NVS_STRING, (void *) &default_settings.ntp_server2, 40, RESTART},
		{"ntp_server3",		(void *) &settings.ntp_server3,		NVS_STRING, (void *) &default_settings.ntp_server3, 40, RESTART},
		{"push1_protocol",	(void *) &settings.publish_server[0].push_protocol,	NVS_INT8,	(void *) &default_settings.publish_server[0].push_protocol, 0, RESTART},
		{"push1_resp_time",	(void *) &settings.publish_server[0].push_resp_time,	NVS_INT8,	(void *) &default_settings.publish_server[0].push_resp_time, 0, RESTART},
		{"mqtt1_server",	(void *) &settings.publish_server[0].mqtt_server,		NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_server, 60, RESTART},
		{"mqtt1_port",		(void *) &settings.publish_server[0].mqtt_port,		NVS_INT32,	(void *) &default_settings.publish_server[0].mqtt_port, 0, RESTART},
		{"mqtt1_cert_file",	(void *) &settings.publish_server[0].mqtt_cert_file,	NVS_STRING,	(void *) &default_settings.publish_server[0].mqtt_cert_file, 32, RESTART},
		{"mqtt1_cli_cert",	(void *) &settings.publish_server[0].mqtt_client_cert_file,	NVS_STRING,	(void *) &default_settings.publish_server[0].mqtt_client_cert_file, 32, RESTART},
		{"mqtt1_key_file",	(void *) &settings.publish_server[0].mqtt_key_file,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_key_file, 32, RESTART},
		{"mqtt1_roottopic",	(void *) &settings.publish_server[0].mqtt_root_topic,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_root_topic, 32, RESTART},
		{"mqtt1_sub_topic",	(void *) &settings.publish_server[0].mqtt_sub_topic,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_sub_topic, 32, RESTART},
		{"mqtt1_pub_topic",	(void *) &settings.publish_server[0].mqtt_pub_topic,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_pub_topic, 32, RESTART},
		{"mqtt1_username",	(void *) &settings.publish_server[0].mqtt_username,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_username, 32, RESTART},
		{"mqtt1_password",	(void *) &settings.publish_server[0].mqtt_password,	NVS_STRING, (void *) &default_settings.publish_server[0].mqtt_password, 32, RESTART},
		{"mqtt1_qos",		(void *) &settings.publish_server[0].mqtt_qos, 		NVS_UINT8,	(void *) &default_settings.publish_server[0].mqtt_qos, 0, NO_RESTART},
		{"mqtt1_tls",		(void *) &settings.publish_server[0].mqtt_tls, 		NVS_INT8,	(void *) &default_settings.publish_server[0].mqtt_tls, 0, RESTART},
		{"push2_protocol",	(void *) &settings.publish_server[1].push_protocol,	NVS_INT8,	(void *) &default_settings.publish_server[1].push_protocol, 0, RESTART},
		{"push2_resp_time",	(void *) &settings.publish_server[1].push_resp_time,	NVS_INT8,	(void *) &default_settings.publish_server[1].push_resp_time, 0, RESTART},
		{"mqtt2_server",	(void *) &settings.publish_server[1].mqtt_server,		NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_server, 60, RESTART},
		{"mqtt2_port",		(void *) &settings.publish_server[1].mqtt_port,		NVS_INT32,	(void *) &default_settings.publish_server[1].mqtt_port, 0, RESTART},
		{"mqtt2_cert_file",	(void *) &settings.publish_server[1].mqtt_cert_file,	NVS_STRING,	(void *) &default_settings.publish_server[1].mqtt_cert_file, 32, RESTART},
		{"mqtt2_cli_cert",	(void *) &settings.publish_server[1].mqtt_client_cert_file,	NVS_STRING,	(void *) &default_settings.publish_server[1].mqtt_client_cert_file, 32, RESTART},
		{"mqtt2_key_file",	(void *) &settings.publish_server[1].mqtt_key_file,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_key_file, 32, RESTART},
		{"mqtt2_roottopic",	(void *) &settings.publish_server[1].mqtt_root_topic,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_root_topic, 32, RESTART},
		{"mqtt2_sub_topic",	(void *) &settings.publish_server[1].mqtt_sub_topic,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_sub_topic, 32, RESTART},
		{"mqtt2_pub_topic",	(void *) &settings.publish_server[1].mqtt_pub_topic,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_pub_topic, 32, RESTART},
		{"mqtt2_username",	(void *) &settings.publish_server[1].mqtt_username,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_username, 32, RESTART},
		{"mqtt2_password",	(void *) &settings.publish_server[1].mqtt_password,	NVS_STRING, (void *) &default_settings.publish_server[1].mqtt_password, 32, RESTART},
		{"mqtt2_qos",		(void *) &settings.publish_server[1].mqtt_qos, 		NVS_UINT8,	(void *) &default_settings.publish_server[1].mqtt_qos, 0, NO_RESTART},
		{"mqtt2_tls",		(void *) &settings.publish_server[1].mqtt_tls, 		NVS_INT8,	(void *) &default_settings.publish_server[1].mqtt_tls, 0, RESTART},
		{"connection_mode",	(void *) &settings.connection_mode,	NVS_INT8,	(void *) &default_settings.connection_mode, 0, RESTART},
		{"wifi_max_retry",	(void *) &settings.wifi_max_retry,	NVS_INT8,	(void *) &default_settings.wifi_max_retry, 0, RESTART},
		{"sg_modbus_addr",	(void *) &settings.sg_modbus_addr,	NVS_UINT8,	(void *) &default_settings.sg_modbus_addr, 0, NO_RESTART},
		{"tcp_port",		(void *) &settings.tcp_port,		NVS_UINT16,	(void *) &default_settings.tcp_port, 0, RESTART},
		{"http_port",		(void *) &settings.http_port,		NVS_UINT16,	(void *) &default_settings.http_port, 0, RESTART},
	//	{"debug_console",	(void *) &settings.debug_console,	NVS_UINT8,	(void *) &default_settings.debug_console},
		{"timezone",		(void *) &settings.timezone,		NVS_INT16,	(void *) &default_settings.timezone, 0, RESTART},
		{"time_sync_src",	(void *) &settings.time_sync_src,	NVS_UINT8,	(void *) &default_settings.time_sync_src, 0, RESTART},

#ifdef ENABLE_LEFT_IR_PORT
		{"left_ir_enabled",	(void *) &settings.left_ir_enabled,	NVS_INT8,	(void *) &default_settings.left_ir_enabled, 0, RESTART},
		{"ir_counter_addr",	(void *) &settings.ir_counter_addr,	NVS_INT8,	(void *) &default_settings.ir_counter_addr, 0, NO_RESTART},
		{"leftir_pushlink",	(void *) &settings.left_ir_push.push_link,	NVS_INT8,	(void *) &default_settings.left_ir_push.push_link, 0, RESTART},
		{"leftir_pushintv",	(void *) &settings.left_ir_push.push_interval,	NVS_UINT16,	(void *) &default_settings.left_ir_push.push_interval, 0, RESTART},
		{"left_ir_desc",	(void *) &settings.left_ir_desc,	NVS_STRING,	(void *) &default_settings.left_ir_desc, 20, NO_RESTART},
#endif
#ifdef ENABLE_RIGHT_IR_PORT
		{"ir_ext_rel_mode",	(void *) &settings.ir_ext_rel_mode,	NVS_INT8,	(void *) &default_settings.ir_ext_rel_mode, 0, RESTART},
		{"ir_relay_desc",	(void *) &settings.ir_relay_description, NVS_STRING, (void *) &default_settings.ir_relay_description, 20, NO_RESTART},
		{"rightir_pushlnk",	(void *) &settings.right_ir_push.push_link,	NVS_INT8,	(void *) &default_settings.right_ir_push.push_link, 0, RESTART},
		{"rightir_pushint",	(void *) &settings.right_ir_push.push_interval,	NVS_UINT16,	(void *) &default_settings.right_ir_push.push_interval, 0, RESTART},
#endif
#ifdef ENABLE_RS485_PORT
		{"rs485_baud_rate",	(void *) &settings.rs485_baud_rate,	NVS_INT8,	(void *) &default_settings.rs485_baud_rate, 0, RESTART},
		{"rs485_stop_bits",	(void *) &settings.rs485_stop_bits,	NVS_INT8,	(void *) &default_settings.rs485_stop_bits, 0, RESTART},
		{"rs485_parity",	(void *) &settings.rs485_parity,	NVS_INT8,	(void *) &default_settings.rs485_parity, 0, RESTART},
		{"rs485_data_bits",	(void *) &settings.rs485_data_bits,	NVS_INT8,	(void *) &default_settings.rs485_data_bits, 0, RESTART},
		{"rs485_type_1",	(void *) &settings.rs485[0].type,	NVS_INT8,	(void *) &default_settings.rs485[0].type, 0, RESTART},
		{"rs485_type_2",	(void *) &settings.rs485[1].type,	NVS_INT8,	(void *) &default_settings.rs485[1].type, 0, RESTART},
		{"rs485_type_3",	(void *) &settings.rs485[2].type,	NVS_INT8,	(void *) &default_settings.rs485[2].type, 0, RESTART},
		{"rs485_type_4",	(void *) &settings.rs485[3].type,	NVS_INT8,	(void *) &default_settings.rs485[3].type, 0, RESTART},
		{"rs485_type_5",	(void *) &settings.rs485[4].type,	NVS_INT8,	(void *) &default_settings.rs485[4].type, 0, RESTART},
		{"rs485_type_6",	(void *) &settings.rs485[5].type,	NVS_INT8,	(void *) &default_settings.rs485[5].type, 0, RESTART},
		{"rs485_type_7",	(void *) &settings.rs485[6].type,	NVS_INT8,	(void *) &default_settings.rs485[6].type, 0, RESTART},
		{"rs485_type_8",	(void *) &settings.rs485[7].type,	NVS_INT8,	(void *) &default_settings.rs485[7].type, 0, RESTART},
		{"rs485_type_9",	(void *) &settings.rs485[8].type,	NVS_INT8,	(void *) &default_settings.rs485[8].type, 0, RESTART},
		{"rs485_type_10",	(void *) &settings.rs485[9].type,	NVS_INT8,	(void *) &default_settings.rs485[9].type, 0, RESTART},
		{"rs485_type_11",	(void *) &settings.rs485[10].type,	NVS_INT8,	(void *) &default_settings.rs485[10].type, 0, RESTART},
		{"rs485_type_12",	(void *) &settings.rs485[11].type,	NVS_INT8,	(void *) &default_settings.rs485[11].type, 0, RESTART},
		{"rs485_type_13",	(void *) &settings.rs485[12].type,	NVS_INT8,	(void *) &default_settings.rs485[12].type, 0, RESTART},
		{"rs485_type_14",	(void *) &settings.rs485[13].type,	NVS_INT8,	(void *) &default_settings.rs485[13].type, 0, RESTART},
		{"rs485_type_15",	(void *) &settings.rs485[14].type,	NVS_INT8,	(void *) &default_settings.rs485[14].type, 0, RESTART},
		{"rs485_type_16",	(void *) &settings.rs485[15].type,	NVS_INT8,	(void *) &default_settings.rs485[15].type, 0, RESTART},
		{"rs485_addr_1",	(void *) &settings.rs485[0].address,NVS_UINT8,	(void *) &default_settings.rs485[0].address, 0, NO_RESTART},
		{"rs485_addr_2",	(void *) &settings.rs485[1].address,NVS_UINT8,	(void *) &default_settings.rs485[1].address, 0, NO_RESTART},
		{"rs485_addr_3",	(void *) &settings.rs485[2].address,NVS_UINT8,	(void *) &default_settings.rs485[2].address, 0, NO_RESTART},
		{"rs485_addr_4",	(void *) &settings.rs485[3].address,NVS_UINT8,	(void *) &default_settings.rs485[3].address, 0, NO_RESTART},
		{"rs485_addr_5",	(void *) &settings.rs485[4].address,NVS_UINT8,	(void *) &default_settings.rs485[4].address, 0, NO_RESTART},
		{"rs485_addr_6",	(void *) &settings.rs485[5].address,NVS_UINT8,	(void *) &default_settings.rs485[5].address, 0, NO_RESTART},
		{"rs485_addr_7",	(void *) &settings.rs485[6].address,NVS_UINT8,	(void *) &default_settings.rs485[6].address, 0, NO_RESTART},
		{"rs485_addr_8",	(void *) &settings.rs485[7].address,NVS_UINT8,	(void *) &default_settings.rs485[7].address, 0, NO_RESTART},
		{"rs485_addr_9",	(void *) &settings.rs485[8].address,NVS_UINT8,	(void *) &default_settings.rs485[8].address, 0, NO_RESTART},
		{"rs485_addr_10",	(void *) &settings.rs485[9].address,NVS_UINT8,	(void *) &default_settings.rs485[9].address, 0, NO_RESTART},
		{"rs485_addr_11",	(void *) &settings.rs485[10].address,NVS_UINT8,	(void *) &default_settings.rs485[10].address, 0, NO_RESTART},
		{"rs485_addr_12",	(void *) &settings.rs485[11].address,NVS_UINT8,	(void *) &default_settings.rs485[11].address, 0, NO_RESTART},
		{"rs485_addr_13",	(void *) &settings.rs485[12].address,NVS_UINT8,	(void *) &default_settings.rs485[12].address, 0, NO_RESTART},
		{"rs485_addr_14",	(void *) &settings.rs485[13].address,NVS_UINT8,	(void *) &default_settings.rs485[13].address, 0, NO_RESTART},
		{"rs485_addr_15",	(void *) &settings.rs485[14].address,NVS_UINT8,	(void *) &default_settings.rs485[14].address, 0, NO_RESTART},
		{"rs485_addr_16",	(void *) &settings.rs485[15].address,NVS_UINT8,	(void *) &default_settings.rs485[15].address, 0, NO_RESTART},
		{"rs485_desc_1",	(void *) &settings.rs485[0].description,NVS_STRING,	(void *) &default_settings.rs485[0].description, 20, NO_RESTART},
		{"rs485_desc_2",	(void *) &settings.rs485[1].description,NVS_STRING,	(void *) &default_settings.rs485[1].description, 20, NO_RESTART},
		{"rs485_desc_3",	(void *) &settings.rs485[2].description,NVS_STRING,	(void *) &default_settings.rs485[2].description, 20, NO_RESTART},
		{"rs485_desc_4",	(void *) &settings.rs485[3].description,NVS_STRING,	(void *) &default_settings.rs485[3].description, 20, NO_RESTART},
		{"rs485_desc_5",	(void *) &settings.rs485[4].description,NVS_STRING,	(void *) &default_settings.rs485[4].description, 20, NO_RESTART},
		{"rs485_desc_6",	(void *) &settings.rs485[5].description,NVS_STRING,	(void *) &default_settings.rs485[5].description, 20, NO_RESTART},
		{"rs485_desc_7",	(void *) &settings.rs485[6].description,NVS_STRING,	(void *) &default_settings.rs485[6].description, 20, NO_RESTART},
		{"rs485_desc_8",	(void *) &settings.rs485[7].description,NVS_STRING,	(void *) &default_settings.rs485[7].description, 20, NO_RESTART},
		{"rs485_desc_9",	(void *) &settings.rs485[8].description,NVS_STRING,	(void *) &default_settings.rs485[8].description, 20, NO_RESTART},
		{"rs485_desc_10",	(void *) &settings.rs485[9].description,NVS_STRING,	(void *) &default_settings.rs485[9].description, 20, NO_RESTART},
		{"rs485_desc_11",	(void *) &settings.rs485[10].description,NVS_STRING,	(void *) &default_settings.rs485[10].description, 20, NO_RESTART},
		{"rs485_desc_12",	(void *) &settings.rs485[11].description,NVS_STRING,	(void *) &default_settings.rs485[11].description, 20, NO_RESTART},
		{"rs485_desc_13",	(void *) &settings.rs485[12].description,NVS_STRING,	(void *) &default_settings.rs485[12].description, 20, NO_RESTART},
		{"rs485_desc_14",	(void *) &settings.rs485[13].description,NVS_STRING,	(void *) &default_settings.rs485[13].description, 20, NO_RESTART},
		{"rs485_desc_15",	(void *) &settings.rs485[14].description,NVS_STRING,	(void *) &default_settings.rs485[14].description, 20, NO_RESTART},
		{"rs485_desc_16",	(void *) &settings.rs485[15].description,NVS_STRING,	(void *) &default_settings.rs485[15].description, 20, NO_RESTART},
		{"rs485_pushlnk1",	(void *) &settings.rs485_push[0].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[0].push_link, 0, RESTART},
		{"rs485_pushlnk2",	(void *) &settings.rs485_push[1].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[1].push_link, 0, RESTART},
		{"rs485_pushlnk3",	(void *) &settings.rs485_push[2].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[2].push_link, 0, RESTART},
		{"rs485_pushlnk4",	(void *) &settings.rs485_push[3].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[3].push_link, 0, RESTART},
		{"rs485_pushlnk5",	(void *) &settings.rs485_push[4].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[4].push_link, 0, RESTART},
		{"rs485_pushlnk6",	(void *) &settings.rs485_push[5].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[5].push_link, 0, RESTART},
		{"rs485_pushlnk7",	(void *) &settings.rs485_push[6].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[6].push_link, 0, RESTART},
		{"rs485_pushlnk8",	(void *) &settings.rs485_push[7].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[7].push_link, 0, RESTART},
		{"rs485_pushlnk9",	(void *) &settings.rs485_push[8].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[8].push_link, 0, RESTART},
		{"rs485_pushlnk10",	(void *) &settings.rs485_push[9].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[9].push_link, 0, RESTART},
		{"rs485_pushlnk11",	(void *) &settings.rs485_push[10].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[10].push_link, 0, RESTART},
		{"rs485_pushlnk12",	(void *) &settings.rs485_push[11].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[11].push_link, 0, RESTART},
		{"rs485_pushlnk13",	(void *) &settings.rs485_push[12].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[12].push_link, 0, RESTART},
		{"rs485_pushlnk14",	(void *) &settings.rs485_push[13].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[13].push_link, 0, RESTART},
		{"rs485_pushlnk15",	(void *) &settings.rs485_push[14].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[14].push_link, 0, RESTART},
		{"rs485_pushlnk16",	(void *) &settings.rs485_push[15].push_link,	NVS_INT8,	(void *) &default_settings.rs485_push[15].push_link, 0, RESTART},
		{"rs485_pushint1",	(void *) &settings.rs485_push[0].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[0].push_interval, 0, RESTART},
		{"rs485_pushint2",	(void *) &settings.rs485_push[1].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[1].push_interval, 0, RESTART},
		{"rs485_pushint3",	(void *) &settings.rs485_push[2].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[2].push_interval, 0, RESTART},
		{"rs485_pushint4",	(void *) &settings.rs485_push[3].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[3].push_interval, 0, RESTART},
		{"rs485_pushint5",	(void *) &settings.rs485_push[4].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[4].push_interval, 0, RESTART},
		{"rs485_pushint6",	(void *) &settings.rs485_push[5].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[5].push_interval, 0, RESTART},
		{"rs485_pushint7",	(void *) &settings.rs485_push[6].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[6].push_interval, 0, RESTART},
		{"rs485_pushint8",	(void *) &settings.rs485_push[7].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[7].push_interval, 0, RESTART},
		{"rs485_pushint9",	(void *) &settings.rs485_push[8].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[8].push_interval, 0, RESTART},
		{"rs485_pushint10",	(void *) &settings.rs485_push[9].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[9].push_interval, 0, RESTART},
		{"rs485_pushint11",	(void *) &settings.rs485_push[10].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[10].push_interval, 0, RESTART},
		{"rs485_pushint12",	(void *) &settings.rs485_push[11].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[11].push_interval, 0, RESTART},
		{"rs485_pushint13",	(void *) &settings.rs485_push[12].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[12].push_interval, 0, RESTART},
		{"rs485_pushint14",	(void *) &settings.rs485_push[13].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[13].push_interval, 0, RESTART},
		{"rs485_pushint15",	(void *) &settings.rs485_push[14].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[14].push_interval, 0, RESTART},
		{"rs485_pushint16",	(void *) &settings.rs485_push[15].push_interval,	NVS_UINT16,	(void *) &default_settings.rs485_push[15].push_interval, 0, RESTART},
#endif
		{"log_disable1",	(void *) &settings.log_disable1,	NVS_UINT16,	(void *) &default_settings.log_disable1, 0, RESTART},
		{"log_disable2",	(void *) &settings.log_disable2,	NVS_UINT16,	(void *) &default_settings.log_disable2, 0, RESTART},
		{"mqtt1_loglevel",	(void *) &settings.mqtt1_loglevel,	NVS_UINT8,	(void *) &default_settings.mqtt1_loglevel, 0, NO_RESTART},
		{"mqtt2_loglevel",	(void *) &settings.mqtt2_loglevel,	NVS_UINT8,	(void *) &default_settings.mqtt2_loglevel, 0, NO_RESTART},
#ifdef ENABLE_PULSE_COUNTER
		{"pulse_cnt_en",	(void *) &settings.pulse_cnt_enabled,NVS_UINT8,	(void *) &default_settings.pulse_cnt_enabled, 0, RESTART},
		{"pcnt_threshold",	(void *) &settings.pcnt_threshold,	NVS_UINT16,	(void *) &default_settings.pcnt_threshold, 0, RESTART},
#endif
#ifdef DEMO_BOX
		{"temp_min_limit",	(void *) &settings.temp_min_limit,	NVS_INT16,	(void *) &default_settings.temp_min_limit, 0, NO_RESTART},
		{"temp_max_limit",	(void *) &settings.temp_max_limit,	NVS_INT16,	(void *) &default_settings.temp_max_limit, 0, NO_RESTART},
#endif
		{"tcp_modbus_en",	(void *) &settings.tcp_modbus_enabled,	NVS_UINT8,	(void *) &default_settings.tcp_modbus_enabled, 0, RESTART},
		{"udp_enabled",		(void *) &settings.udp_enabled,	NVS_UINT8,	(void *) &default_settings.udp_enabled, 0, RESTART},
#ifdef ENABLE_PT1000
		{"adc_enabled",		(void *) &settings.adc_enabled,	NVS_UINT8,	(void *) &default_settings.adc_enabled, 0, RESTART},
#endif
		{"mdns_enabled",	(void *) &settings.mdns_enabled,	NVS_UINT8,	(void *) &default_settings.mdns_enabled, 0, RESTART},
		//{"modbus_sl_en",	(void *) &settings.modbus_slave_enabled,	NVS_UINT8,	(void *) &default_settings.modbus_slave_enabled, 0, RESTART},
		{"cntrl_task_en",	(void *) &settings.control_task_enabled,	NVS_UINT8,	(void *) &default_settings.control_task_enabled, 0, RESTART},
#ifdef EEPROM_STORAGE
		{"energy_log_1",	(void *) &settings.energy_log[0],	NVS_UINT8,	(void *) &default_settings.energy_log[0], 0, RESTART},
		{"energy_log_2",	(void *) &settings.energy_log[1],	NVS_UINT8,	(void *) &default_settings.energy_log[1], 0, RESTART},
		{"energy_log_3",	(void *) &settings.energy_log[2],	NVS_UINT8,	(void *) &default_settings.energy_log[2], 0, RESTART},
		{"energy_log_4",	(void *) &settings.energy_log[3],	NVS_UINT8,	(void *) &default_settings.energy_log[3], 0, RESTART},
#endif

		{"wifi_ap_enabled",	(void *) &settings.wifi_ap_enabled,	NVS_UINT8,	(void *) &default_settings.wifi_ap_enabled, 0, RESTART},
#ifdef STATIC_IP
		{"static_ip_en",	(void *) &settings.static_ip_enabled,	NVS_UINT8,	(void *) &default_settings.static_ip_enabled, 0, RESTART},
		{"ip",		(void *) &settings.ip,		NVS_STRING,	(void *) &default_settings.ip, 16, RESTART},
		{"gateway",		(void *) &settings.gateway,		NVS_STRING,	(void *) &default_settings.gateway, 16, RESTART},
		{"netmask",		(void *) &settings.netmask,		NVS_STRING,	(void *) &default_settings.netmask, 16, RESTART},
		{"dns1",		(void *) &settings.dns1,		NVS_STRING,	(void *) &default_settings.dns1, 16, RESTART},
		{"dns2",		(void *) &settings.dns2,		NVS_STRING,	(void *) &default_settings.dns2, 16, RESTART},
#endif
		{ "\0",						0, NVS_STRING, 0, 0, NO_RESTART}, //terminator
};

void clear_all_keys(nvs_handle my_handle)
{
	//size_t required_size = 0;
	MY_LOGE(TAG, "Clearing all settings!!!!");
#if 0 //20.4.2021:dont read it because it can create loop
	//try to save factory settings
	nvs_get_str(my_handle, "model_type", NULL, &required_size);
	if(required_size > 0 && required_size < 17)
		nvs_get_str(my_handle, "model_type", factory_settings.model_type, &required_size);

	nvs_get_str(my_handle, "serial_number", NULL, &required_size);
	if(required_size > 0 && required_size < 9)
		nvs_get_str(my_handle, "serial_number", factory_settings.serial_number, &required_size);

	read_i8_key_nvs("hw_ver", &factory_settings.hw_ver);
#endif
	MY_LOGI(TAG, "nvs_flash_erase");
	//nvs_erase_all(my_handle);
	ESP_ERROR_CHECK(nvs_flash_erase()); //20.4.2021: try with complete erase
	vTaskDelay(500 / portTICK_PERIOD_MS);
#if 0
	MY_LOGI(TAG, "nvs_commit");
	nvs_commit(my_handle);
	vTaskDelay(500 / portTICK_PERIOD_MS);
	MY_LOGI(TAG, "nvs_close");
	nvs_close(my_handle);
	MY_LOGI(TAG, "nvs_flash_deinit");
	nvs_flash_deinit();
#endif

	MY_LOGI(TAG, "nvs_flash_init");
	//nvs_flash_init();
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		MY_LOGE(TAG, "nvs_flash_init Failed:%d. Erasing NVS", err);
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );
	//nvs_commit(my_handle);

	//save additional settings to newly formatted nvs storage
	MY_LOGI(TAG, "saving factory settings");
	save_string_key_nvs("serial_number", factory_settings.serial_number);
	save_string_key_nvs("model_type", factory_settings.model_type);
	save_i8_key_nvs("hw_ver", factory_settings.hw_ver);
	save_i8_key_nvs("debug_console", debug_console); //save debug_console setting
	save_i32_key_nvs("upgrade_count", status.upgrade.count);
	save_i8_key_nvs(BLE_PROV_NVS_KEY, 0); //reset provisioning flag

	save_active_settings();

	reset_device();
}

int save_settings_nvs(int restart_enabled)
{
	esp_err_t err;
	int i;
	int restart = 0;

	// Open
	//printf("\n");
	ESP_LOGI(TAG,"Opening Non-Volatile Storage (NVS) handle... ");
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}
	ESP_LOGI(TAG,"Done");

	// Write
	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		if(nvs_settings_array[i].type == NVS_INT32)
		{
			int32_t tmp;
			err= nvs_get_i32(my_handle, nvs_settings_array[i].name, &tmp);
			if(tmp != *(int32_t *)nvs_settings_array[i].value)
			{
				printf("Updating INT32 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
				err = nvs_set_i32(my_handle, nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
		}
		if(nvs_settings_array[i].type == NVS_INT16)
		{
			int16_t tmp;
			err= nvs_get_i16(my_handle, nvs_settings_array[i].name, &tmp);
			if(tmp != *(int16_t *)nvs_settings_array[i].value)
			{
				printf("Updating INT16 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
				err = nvs_set_i16(my_handle, nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
		}
		if(nvs_settings_array[i].type == NVS_UINT16)
		{
			uint16_t tmp;
			err= nvs_get_u16(my_handle, nvs_settings_array[i].name, &tmp);
			if(tmp != *(uint16_t *)nvs_settings_array[i].value)
			{
				printf("Updating UINT16 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].value);
				err = nvs_set_u16(my_handle, nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
		}
		if(nvs_settings_array[i].type == NVS_INT8)
		{
			int8_t tmp;
			err= nvs_get_i8(my_handle, nvs_settings_array[i].name, &tmp);
			if(tmp != *(int8_t *)nvs_settings_array[i].value)
			{
				printf("Updating INT8 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
				err = nvs_set_i8(my_handle, nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
		}
		else if(nvs_settings_array[i].type == NVS_UINT8)
		{
			uint8_t tmp;
			err= nvs_get_u8(my_handle, nvs_settings_array[i].name, &tmp);
			if(tmp != *(uint8_t *)nvs_settings_array[i].value)
			{
				printf("Updating UINT8 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].value);
				err = nvs_set_u8(my_handle, nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
		}
		else if(nvs_settings_array[i].type == NVS_STRING)
		{
			//terminate_string((char *)nvs_settings_array[i].value, strlen((char *)nvs_settings_array[i].value)); //MiQen fills strings with spaces and not terminate them
			terminate_string((char *)nvs_settings_array[i].value, nvs_settings_array[i].string_size); //MiQen fills strings with spaces and not terminate them

			size_t required_size = 0;
			nvs_get_str(my_handle, nvs_settings_array[i].name, NULL, &required_size);
			char *tmp = malloc(required_size + 1);
			if(required_size > 0 && required_size < NVS_STRING_MAX_LEN)
				err = nvs_get_str(my_handle, nvs_settings_array[i].name, tmp, &required_size);

			if(strncmp(tmp, (char *)nvs_settings_array[i].value, nvs_settings_array[i].string_size))
			{
				printf("Updating STRING %s in NVS, from:%s to:%s size:%d strlen:%d.\n", nvs_settings_array[i].name, tmp, (char *)nvs_settings_array[i].value, nvs_settings_array[i].string_size, strlen(nvs_settings_array[i].value));
				err = nvs_set_str(my_handle, nvs_settings_array[i].name,  (char *)nvs_settings_array[i].value);
				if(nvs_settings_array[i].reset_required)
					restart++;
			}
			free(tmp);
		}

		//printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
		if (err != ESP_OK) {
			MY_LOGE(TAG, "Error (%s) updating %s!", esp_err_to_name(err), nvs_settings_array[i].name);
			statistics.errors++;

			if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
			{
				clear_all_keys(my_handle);//clear nvs and reboot
				return err;
			}
			// Close
#if 0 //20.3.2020: continue to save other settings
			nvs_close(my_handle);
			return err;
#endif
		}
	}

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	ESP_LOGI(TAG,"Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

	// Close
	nvs_close(my_handle);

	if(err == ESP_OK)
	{
		status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
		MY_LOGI(TAG,"Save settings OK. Size:%d CRC:%04X", sizeof(settings), status.settings_crc);
		save_u16_key_nvs("settings_crc", status.settings_crc);
		status.nvs_settings_crc = status.settings_crc;
	}
	else
	{
		MY_LOGE(TAG,"Save settings ERROR.");
		statistics.errors++;
	}

	if(restart_enabled && restart)
	{
		reset_device();
	}

	return err;
}

int save_active_settings()
{
	esp_err_t err = ESP_OK;
	int i;

	ESP_LOGI(TAG,"Opening Non-Volatile Storage (NVS) handle... ");
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}
	ESP_LOGI(TAG,"Done");

	// Write
	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		if(nvs_settings_array[i].type == NVS_INT32)
		{
			printf("Updating INT32 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
			err = nvs_set_i32(my_handle, nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
		}
		if(nvs_settings_array[i].type == NVS_INT16)
		{
			printf("Updating INT16 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
			err = nvs_set_i16(my_handle, nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
		}
		if(nvs_settings_array[i].type == NVS_UINT16)
		{
			printf("Updating UINT16 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].value);
			err = nvs_set_u16(my_handle, nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].value);
		}
		if(nvs_settings_array[i].type == NVS_INT8)
		{
			printf("Updating INT8 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
			err = nvs_set_i8(my_handle, nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_UINT8)
		{
			printf("Updating UINT8 %s in NVS, value: %d\n", nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].value);
			err = nvs_set_u8(my_handle, nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_STRING)
		{
			printf("Updating STRING %s in NVS, to:%s size:%d strlen:%d.\n",
					nvs_settings_array[i].name, (char *)nvs_settings_array[i].value, nvs_settings_array[i].string_size, strlen(nvs_settings_array[i].value));
			err = nvs_set_str(my_handle, nvs_settings_array[i].name,  (char *)nvs_settings_array[i].value);
		}

		//printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
		if (err != ESP_OK) {
			MY_LOGE(TAG, "Error (%s) updating %s!", esp_err_to_name(err), nvs_settings_array[i].name);
			statistics.errors++;

			//if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
				//clear_all_keys(my_handle);//clear nvs and reboot
			// Close
		}
	}

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	ESP_LOGI(TAG,"Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

	// Close
	nvs_close(my_handle);

	if(err == ESP_OK)
	{
		status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
		MY_LOGI(TAG,"Save settings OK. Size:%d CRC:%04X", sizeof(settings), status.settings_crc);
		save_u16_key_nvs("settings_crc", status.settings_crc);
		status.nvs_settings_crc = status.settings_crc;
	}
	else
	{
		MY_LOGE(TAG,"Save settings ERROR.");
		statistics.errors++;
	}

	return err;
}

int read_settings_nvs()
{
	esp_err_t err;
	int i;
	int not_found_flag = 0;

	// Open
	//printf("\n");
	ESP_LOGI(TAG,"Opening Non-Volatile Storage (NVS) handle... ");
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}
	ESP_LOGI(TAG,"Done");

	//read factory settings
	size_t required_size = 0;
	nvs_get_str(my_handle, "model_type", NULL, &required_size);
	//printf("String %s size:%d ", "model_type", required_size);
	if(required_size > 0 && required_size < NVS_STRING_MAX_LEN)
		err = nvs_get_str(my_handle, "model_type", factory_settings.model_type, &required_size);
	else
	{
		printf("\n");
		MY_LOGE(TAG, "Error (%s) size: %d", "model_type", required_size);
		err = ESP_ERR_NVS_NOT_FOUND; //todo: better error code
	}
	if(err == ESP_OK)
		printf("model_type = %s\n\r", factory_settings.model_type);
	else if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "model_type is not initialized yet! Using default value");
		//memcpy(factory_settings.model_type, DEFAULT_MODEL_TYPE, 16);
		factory_settings.model_type[0] = 0;
	}

	//serial number
	nvs_get_str(my_handle, "serial_number", NULL, &required_size);
	//printf("String %s size:%d ", "serial_number", required_size);
	if(required_size > 0 && required_size < NVS_STRING_MAX_LEN)
		err = nvs_get_str(my_handle, "serial_number", factory_settings.serial_number, &required_size);
	else
	{
		printf("\n");
		ESP_LOGW(TAG, "Error (%s) size: %d", "serial_number", required_size);
		err = ESP_ERR_NVS_NOT_FOUND; //todo: better error code
	}
	if(err == ESP_OK)
		printf("serial_number = %s\n\r", factory_settings.serial_number);
	else if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "serial_number is not initialized yet! Using default value");
		//memcpy(factory_settings.serial_number, CONFIG_SERIAL_NUMBER, 8);
		factory_settings.serial_number[0] = 0;
	}

	// Read settings structure
	//for(i = 0; i < sizeof(nvs_settings_array)/sizeof(nvs_settings_t); i++) {
	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		//printf("Reading %s from NVS... ", nvs_settings_array[i].name);
		if(nvs_settings_array[i].type == NVS_INT32)
		{
			err = nvs_get_i32(my_handle, nvs_settings_array[i].name, (int32_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_INT16)
		{
			err = nvs_get_i16(my_handle, nvs_settings_array[i].name, (int16_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_UINT16)
		{
			err = nvs_get_u16(my_handle, nvs_settings_array[i].name, (uint16_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_INT8)
		{
			err = nvs_get_i8(my_handle, nvs_settings_array[i].name, (int8_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_UINT8)
		{
			err = nvs_get_u8(my_handle, nvs_settings_array[i].name, (uint8_t *)nvs_settings_array[i].value);
		}
		else if(nvs_settings_array[i].type == NVS_STRING)
		{
			size_t required_size = 0;
			nvs_get_str(my_handle, nvs_settings_array[i].name, NULL, &required_size);
			//printf("String %s size:%d ", nvs_settings_array[i].name, required_size);
			//			if(required_size > size)
			//			{
			//				printf("ERROR req size:%d > available size:%d", required_size, size);
			//				// Close
			//				nvs_close(my_handle);
			//				return -1;
			//			}
			if(required_size > 0 && required_size < NVS_STRING_MAX_LEN)
				err = nvs_get_str(my_handle, nvs_settings_array[i].name, (char *)nvs_settings_array[i].value, &required_size);
			else
			{
				//printf("\n");
				MY_LOGE(TAG, "Error (%s) size: %d", nvs_settings_array[i].name, required_size);
				statistics.errors++;
				err = ESP_ERR_NVS_NOT_FOUND; //todo: better error code
			}

		}
		switch (err) {
		case ESP_OK:
			//printf("Done\n");
			if(nvs_settings_array[i].type == NVS_STRING)
				printf("%s = %s\n", nvs_settings_array[i].name, (char *)nvs_settings_array[i].value);
			else if(nvs_settings_array[i].type == NVS_INT32)
				printf("%s = %d\n", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
			else if(nvs_settings_array[i].type == NVS_INT16)
				printf("%s = %d\n", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
			else if(nvs_settings_array[i].type == NVS_UINT16)
				printf("%s = %d\n", nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].value);
			else if(nvs_settings_array[i].type == NVS_INT8)
				printf("%s = %d\n", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
			else if(nvs_settings_array[i].type == NVS_UINT8)
				printf("%s = %d\n", nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].value);
			break;
		case ESP_ERR_NVS_NOT_FOUND:
			not_found_flag++;
			ESP_LOGW(TAG, "The value is not initialized yet! Using default value");
			err = ESP_OK;
			if(nvs_settings_array[i].type == NVS_STRING)
			{
				printf("%s = %s\n", nvs_settings_array[i].name, (char *)nvs_settings_array[i].default_value);
				err = nvs_set_str(my_handle, nvs_settings_array[i].name,  (char *)nvs_settings_array[i].default_value);
			}
			else if(nvs_settings_array[i].type == NVS_INT32)
			{
				printf("%s = %d\n", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].default_value);
				err = nvs_set_i32(my_handle, nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].default_value);
			}
			else if(nvs_settings_array[i].type == NVS_INT16)
			{
				printf("%s = %d\n", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].default_value);
				err = nvs_set_i16(my_handle, nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].default_value);
			}
			else if(nvs_settings_array[i].type == NVS_UINT16)
			{
				printf("%s = %d\n", nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].default_value);
				err = nvs_set_u16(my_handle, nvs_settings_array[i].name, *(uint16_t *)nvs_settings_array[i].default_value);
			}
			else if(nvs_settings_array[i].type == NVS_INT8)
			{
				printf("%s = %d\n", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].default_value);
				err = nvs_set_i8(my_handle, nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].default_value);
			}
			else if(nvs_settings_array[i].type == NVS_UINT8)
			{
				printf("%s = %d\n", nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].default_value);
				err = nvs_set_u8(my_handle, nvs_settings_array[i].name, *(uint8_t *)nvs_settings_array[i].default_value);
			}
			printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
			if (err != ESP_OK) {
				MY_LOGE(TAG, "Error (%s) updating %s!", esp_err_to_name(err), nvs_settings_array[i].name);
				if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
				{
					clear_all_keys(my_handle);//clear nvs and reboot
					return err;
				}
				//statistics.errors++;
			}
			break;
		default :
			MY_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
			statistics.errors++;
		}
	}//for settings



	// Close
	nvs_close(my_handle);

	// Example of nvs_get_stats() to get the number of used entries and free entries:
	nvs_stats_t nvs_stats;
	nvs_get_stats(NULL, &nvs_stats);
	printf("NVS Stats: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
			nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);

	if(not_found_flag)
		return ESP_ERR_NOT_FOUND; //flag to read settings again, because some values was uninitialized

	return ESP_OK;
}

esp_err_t clear_nvs_settings()
{
	int i;
	//load default settings
	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		if ( nvs_settings_array[i].type == NVS_STRING)
			strncpy((char *)nvs_settings_array[i].value, (char *)nvs_settings_array[i].default_value, nvs_settings_array[i].string_size);
		else if(nvs_settings_array[i].type == NVS_INT32)
			*(int32_t*)nvs_settings_array[i].value = *(int32_t *)nvs_settings_array[i].default_value;
		else if(nvs_settings_array[i].type == NVS_INT16)
			*(int16_t*)nvs_settings_array[i].value = *(int16_t *)nvs_settings_array[i].default_value;
		else if(nvs_settings_array[i].type == NVS_UINT16)
			*(uint16_t*)nvs_settings_array[i].value = *(uint16_t *)nvs_settings_array[i].default_value;
		else if(nvs_settings_array[i].type == NVS_INT8)
			*(int8_t*)nvs_settings_array[i].value = *(int8_t *)nvs_settings_array[i].default_value;
		else if(nvs_settings_array[i].type == NVS_UINT8)
			*(uint8_t*)nvs_settings_array[i].value = *(uint8_t *)nvs_settings_array[i].default_value;
	}

	//check model type and set default connection
	if(strncmp(factory_settings.model_type, "SG-E1", 5) == 0)
		settings.connection_mode = ETHERNET_MODE;
	else
		settings.connection_mode = WIFI_MODE;

	//set default root topic to serial number
	memcpy(settings.publish_server[0].mqtt_root_topic, factory_settings.serial_number, 8);
	memcpy(settings.publish_server[1].mqtt_root_topic, factory_settings.serial_number, 8);

	//debug console
	save_i8_key_nvs("debug_console", DEBUG_CONSOLE_485);//default is RS485

	save_settings_nvs(NO_RESTART);
	return ESP_OK;
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *get_settings_json(void)
{
	char *string = NULL;
	cJSON *header = NULL;
	cJSON *settings = NULL;
	cJSON *setting_value = NULL;
	size_t i;
	nvs_handle nvs_handle;

	esp_err_t err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(nvs_handle);
		return NULL;
	}

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//create header object
	header = cJSON_CreateObject();
	if (header == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "header", header); //header name
	cJSON_AddItemToObject(header, "cmd", cJSON_CreateString("get_settings"));
	cJSON_AddItemToObject(header, "timestamp", cJSON_CreateString(status.local_time));

	//create settings object
	settings = cJSON_CreateObject();
	if (settings == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "settings", settings);

	//add all settings from table
	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		if(nvs_settings_array[i].type == NVS_INT32)
		{
			int32_t value;
			err = nvs_get_i32(nvs_handle, nvs_settings_array[i].name, &value);
			if(err != ESP_OK )
			{
				ESP_LOGE(TAG, "Error nvs_get_i32 failed: %d", err);
				statistics.errors++;
				continue;
			}
			setting_value = cJSON_CreateNumber(value);
		}
		else if(nvs_settings_array[i].type == NVS_INT16)
		{
			int16_t value;
			err = nvs_get_i16(nvs_handle, nvs_settings_array[i].name, &value);
			if(err != ESP_OK )
			{
				ESP_LOGE(TAG, "Error nvs_get_i16 failed: %d", err);
				statistics.errors++;
				continue;
			}
			setting_value = cJSON_CreateNumber(value);
		}
		else if(nvs_settings_array[i].type == NVS_UINT16)
		{
			uint16_t value;
			err = nvs_get_u16(nvs_handle, nvs_settings_array[i].name, &value);
			if(err != ESP_OK )
			{
				ESP_LOGE(TAG, "Error nvs_get_u16 failed: %d", err);
				statistics.errors++;
				continue;
			}
			setting_value = cJSON_CreateNumber(value);
		}
		else if(nvs_settings_array[i].type == NVS_INT8)
		{
			int8_t value;
			err = nvs_get_i8(nvs_handle, nvs_settings_array[i].name, &value);
			if(err != ESP_OK )
			{
				ESP_LOGE(TAG, "Error nvs_get_i8 failed: %d", err);
				statistics.errors++;
				continue;
			}
			setting_value = cJSON_CreateNumber(value);
		}
		else if(nvs_settings_array[i].type == NVS_UINT8)
		{
			uint8_t value;
			err = nvs_get_u8(nvs_handle, nvs_settings_array[i].name, &value);
			if(err != ESP_OK )
			{
				ESP_LOGE(TAG, "Error nvs_get_u8 failed: %d", err);
				statistics.errors++;
				continue;
			}
			setting_value = cJSON_CreateNumber(value);
		}
		else if(nvs_settings_array[i].type == NVS_STRING)
		{
			size_t required_size = 0;
			char string_value[NVS_STRING_MAX_LEN];
			nvs_get_str(nvs_handle, nvs_settings_array[i].name, NULL, &required_size);
			//ESP_LOGI(TAG, "String %s size:%d ", nvs_settings_array[i].name, required_size);
			if(required_size > 0 && required_size < NVS_STRING_MAX_LEN)
			{
				err = nvs_get_str(nvs_handle, nvs_settings_array[i].name, string_value, &required_size);
				if(err != ESP_OK )
				{
					ESP_LOGE(TAG, "Error nvs_get_str failed: %d", err);
					statistics.errors++;
					continue;
				}
				setting_value = cJSON_CreateString(string_value);
			}
			else
			{
				ESP_LOGE(TAG, "Error (%s) required_size: %d", nvs_settings_array[i].name, required_size);
				statistics.errors++;
				continue;
			}
		}
		else
		{
			ESP_LOGE(TAG, "%s unknown nvs type: %d", __FUNCTION__, nvs_settings_array[i].type);
			statistics.errors++;
			continue;
		}

		if (setting_value == NULL)
		{
			goto end;
		}
		cJSON_AddItemToObject(settings, nvs_settings_array[i].name, setting_value);

	}

	nvs_close(nvs_handle);

	//add factory settings
	cJSON_AddItemToObject(settings, "serial_number", cJSON_CreateString(factory_settings.serial_number));
	cJSON_AddItemToObject(settings, "model_type", cJSON_CreateString(factory_settings.model_type));
	cJSON_AddItemToObject(settings, "eeprom_size", cJSON_CreateNumber(eeprom_size));

	cJSON_AddItemToObject(settings, "debug_console", cJSON_CreateNumber(debug_console - DEBUG_CONSOLE_INTERNAL));//console is not in settings anymore

	//string = cJSON_Print(json_root);
	string = cJSON_PrintUnformatted(json_root);

	if (string == NULL || strlen(string) == 0)
	{
		ESP_LOGE(TAG, "Failed to create settings JSON:%d", strlen(string));
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//ESP_LOGI(TAG, "%s", string);
	return string;
}

esp_err_t save_string_key_nvs(char *name, char *value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	err = nvs_set_str(my_handle, name, value);
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs save str\n", esp_err_to_name(err));
		statistics.errors++;
		if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
		{
			clear_all_keys(my_handle);//clear nvs and reboot
			return err;
		}
	}

	printf("Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
	nvs_close(my_handle);

	return err;
}

esp_err_t read_str_key_nvs(char *name, char *value, size_t required_size)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT8 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);

	err = nvs_get_str(my_handle, name, value, &required_size);
	if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "%s is not set yet. Setting to 0!", name);
		err = nvs_set_str(my_handle, name, "");
	}
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs read str\n", esp_err_to_name(err));
		statistics.errors++;
		if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
		{
			clear_all_keys(my_handle);//clear nvs and reboot
			return err;
		}
	}

	nvs_close(my_handle);

	return err;
}

esp_err_t save_u8_key_nvs(char *name, uint8_t value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT32 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
	err = nvs_set_u8(my_handle, name, value);
	if (err != ESP_OK)
		MY_LOGE(TAG, "Error (%s) nvs save u8\n", esp_err_to_name(err));

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	printf("Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
	nvs_close(my_handle);

	return err;
}

esp_err_t save_i8_key_nvs(char *name, int8_t value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT32 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
	err = nvs_set_i8(my_handle, name, value);
	if (err != ESP_OK)
		MY_LOGE(TAG, "Error (%s) nvs save i8\n", esp_err_to_name(err));

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	printf("Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
	nvs_close(my_handle);

	return err;
}

esp_err_t read_i8_key_nvs(char *name, int8_t *value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Read
	err = nvs_get_i8(my_handle, name, value);
	if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "%s is not set yet. Setting to 0!", name);
		err = nvs_set_i8(my_handle, name, 0);
		value = 0;
	}
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs read i8", esp_err_to_name(err));
		statistics.errors++;
	}

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	nvs_close(my_handle);

	return err;
}

esp_err_t save_u16_key_nvs(char *name, uint16_t value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT16 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
	err = nvs_set_u16(my_handle, name, value);
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs save u16\n", esp_err_to_name(err));
		statistics.errors++;
	}

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	printf("Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
	nvs_close(my_handle);

	return err;
}

esp_err_t read_u16_key_nvs(char *name, uint16_t *value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT16 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int16_t *)nvs_settings_array[i].value);
	err = nvs_get_u16(my_handle, name, value);
	if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "%s is not set yet. Setting to 0!", name);
		err = nvs_set_u16(my_handle, name, 0);
	}
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs read i16\n", esp_err_to_name(err));
		statistics.errors++;
	}

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	nvs_close(my_handle);

	return err;
}

esp_err_t save_i32_key_nvs(char *name, int32_t value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT32 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int32_t *)nvs_settings_array[i].value);
	err = nvs_set_i32(my_handle, name, value);
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs save i32\n", esp_err_to_name(err));
		statistics.errors++;
	}

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	printf("Committing updates in NVS ... ");
	err = nvs_commit(my_handle);
	printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
	nvs_close(my_handle);

	return err;
}

esp_err_t read_i32_key_nvs(char *name, int32_t *value)
{
	esp_err_t err;

	// Open
	nvs_handle my_handle;
	err = nvs_open("nvs", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		statistics.errors++;
		// Close
		nvs_close(my_handle);
		return err;
	}

	// Write
	//printf("Updating INT8 %s in NVS, value: %d ", nvs_settings_array[i].name, *(int8_t *)nvs_settings_array[i].value);
	err = nvs_get_i32(my_handle, name, value);
	if(err == ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGW(TAG, "%s is not set yet. Setting to 0!", name);
		err = nvs_set_i32(my_handle, name, 0);
	}
	if (err != ESP_OK)
	{
		MY_LOGE(TAG, "Error (%s) nvs read i32\n", esp_err_to_name(err));
		statistics.errors++;
	}

	if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE )
	{
		clear_all_keys(my_handle);//clear nvs and reboot
		return err;
	}

	nvs_close(my_handle);

	return err;
}

static void terminate_string(char *str, int len)
{
	int i;

	for (i = len - 1; i >= 0; i--)
	{
		if (str[i] == ' ' || str[i] == 0) //go from end and replace spaces with 0
			str[i] = 0;
		else
			break;    //exit at nonspace char
	}

	int start_to_fill = 0;

	//fill unused place with 0
	for(i = 0; i < len; i++)
	{
		if(str[i] == 0)//1st terminator
			start_to_fill = 1;

		if(start_to_fill)
			str[i] = 0;
	}
}
