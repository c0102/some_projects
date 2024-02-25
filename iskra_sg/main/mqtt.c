/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "main.h"
extern u8_t tcp_obuf[RS485_RX_BUF_SIZE];

static int mqtt_ready[2] = {0, 0};
esp_mqtt_client_handle_t mqtt_client[2];
mqtt_topic_t mqtt_topic[2];

int read_modbus_mqtt(int modbus_address, int reg, int no_regs, int mqtt_instance, char *serial_number);
int write_multi_modbus_mqtt(int modbus_address, int reg, char write_array[], int byte_size, char *serial_number);
int16_t writeModbusReg16(uint16_t in_addr, uint16_t data[], uint16_t len);
int writeModbusReg6(uint16_t addr, uint16_t val);
int write_single_modbus_mqtt(int modbus_address, int reg, uint16_t val, char *serial_number);

static const char *TAG = "MQTT";

int MQTT_LOG(int level, int number, const char *TAG, const char *fmt, ...)
{
    char buffer[128];

    if(level >=ESP_LOG_INFO && checkTagLoglevel(TAG))
    	return 0; //skip publish because log level has been raised

    va_list args;
    va_start(args, fmt);
    sprintf(buffer, "%s:", TAG);
    int rc = vsnprintf(buffer + strlen(TAG) + 1, sizeof(buffer)- strlen(TAG) - 1, fmt, args);
    va_end(args);
    if(rc > 0)
    	mqtt_publish(number, mqtt_topic[number].publish, "log", buffer, 0, settings.publish_server[number].mqtt_qos, 0);
    return rc;
}

int mqtt_publish(int mqtt_index, const char *topic, const char *subtopic, const char *data, int len, int qos, int retain)
{
	int ret = 0;

	if(strlen(data) > CONFIG_MQTT_BUFFER_SIZE)
	{
		MY_LOGE(TAG, "Payload size:%d too big", strlen(data));
		return -1;
	}

	//if(settings.publish_server[mqtt_index].push_enabled == 0)
		//return 0;

	if(mqtt_ready[mqtt_index])
	{
		char full_topic[100];
		sprintf(full_topic, "%s/%s", topic, subtopic);
		//MY_LOGD(TAG, "%s to topic: %s, client: %d", __FUNCTION__, topic, mqtt_index+1);
		//if(settings.mqtt_enabled)
		//MY_LOGI(TAG, "PUB size:%d %s", strlen(data), data);
		int ret = esp_mqtt_client_publish(mqtt_client[mqtt_index], full_topic, data, len, qos, retain);
		if(ret < 0)
		{
			ESP_LOGE(TAG, "esp_mqtt_client_publish %d failed", mqtt_index+1);
			statistics.errors++;
		}
	}
	else
	{
		//MY_LOGI(TAG, "client %d not ready", mqtt_index+1);
		return -1;
	}

	return ret;
}

#ifdef START_MQTT_TASK

#define TOPIC_ENDS_WITH(topic, topic_len, topic_end) \
		(strncmp(&topic[topic_len - strlen(topic_end)], topic_end, strlen(topic_end) - 1) == 0)

static char *mqtt_receive_buffer[2];
//static char mqtt_receive_buffer[2][MQTT_INTERNAL_BUFFER_SIZE];
char chunk_topic[200];

extern char update_url[];
extern char cert_pem[2048];
extern nvs_settings_t nvs_settings_array[];
extern const esp_app_desc_t *app_desc;

void parse_msg(char *topic, int topic_len, char *msg, int msg_len, esp_mqtt_client_handle_t client);
void simple_ota_example_task(void * pvParameter);
char *get_settings_json(void);

#define CERT_MEM_SIZE (3096)
char *mqtt_cert[2];
char *mqtt_cli_cert[2];
char *mqtt_key[2];

static void mqtt_hello_msg(int mqtt_instance)
{
	cJSON *json_root = cJSON_CreateObject();
	char *string = NULL;

	cJSON_AddItemToObject(json_root, "hello", cJSON_CreateString("hello world"));
	cJSON_AddItemToObject(json_root, "serial number", cJSON_CreateString(factory_settings.serial_number));
	cJSON_AddItemToObject(json_root, "model_type", cJSON_CreateString(factory_settings.model_type));
	cJSON_AddItemToObject(json_root, "sw_ver", cJSON_CreateString(app_desc->version));
	cJSON_AddItemToObject(json_root, "hw_ver", cJSON_CreateNumber(factory_settings.hw_ver));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	cJSON_Delete(json_root);

	MY_LOGI(TAG, "%s", string);
	int ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", string, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
	if(ret != ESP_OK)
	{
		MY_LOGE(TAG, "MQTT:%d write_single_modbus_mqtt publish failed :%d", mqtt_instance+1, ret);
		statistics.errors++;
	}
	if(string != NULL)
		free(string);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
	int mqtt_instance = 0; //default is 0
	esp_mqtt_client_handle_t client = event->client;
	int msg_id = 0;
	char strtmp[120];
	// your_context_t *context = event->context;
	if(mqtt_client[1] == client)
		mqtt_instance = 1;

	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		status.mqtt[mqtt_instance].state = event->event_id;
		MY_LOGI(TAG, "MQTT%d_EVENT_CONNECTED, subscribing to: %s", mqtt_instance+1, mqtt_topic[mqtt_instance].subscribe);
		msg_id = esp_mqtt_client_subscribe(client, mqtt_topic[mqtt_instance].subscribe, 0);
		mqtt_ready[mqtt_instance] = 1;
		mqtt_hello_msg(mqtt_instance);
		//sprintf(strtmp, "MQTT Connected. App Version: %s", app_desc->version);
		//mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "log", strtmp, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
		//MY_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_DISCONNECTED:
		status.mqtt[mqtt_instance].state = event->event_id;
		MY_LOGW(TAG, "MQTT%d_EVENT_DISCONNECTED", mqtt_instance+1);
		mqtt_ready[mqtt_instance] = 0;
		break;
	case MQTT_EVENT_SUBSCRIBED:
		sprintf(strtmp, "MQTT%d subscribed to %s", mqtt_instance+1, mqtt_topic[mqtt_instance].subscribe);
		MY_LOGI(TAG, "%s, msg_id=%d", strtmp, event->msg_id);
		char full_topic[70];
		sprintf(full_topic, "%s/%s", mqtt_topic[mqtt_instance].publish, "log");
		msg_id = esp_mqtt_client_publish(client, full_topic, strtmp, 0, 0, 0);
		MY_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		MY_LOGI(TAG, "MQTT%d_EVENT_UNSUBSCRIBED, msg_id=%d", mqtt_instance+1, event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		MY_LOGI(TAG, "MQTT%d_EVENT_PUBLISHED, msg_id=%d", mqtt_instance+1, event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		//MY_LOGI(TAG, "MQTT_EVENT_DATA, TotalDataLen:%d DataLen:%d DataOffset:%d TopicLen:%d, msg_id=%d", event->total_data_len, event->data_len, event->current_data_offset, event->topic_len, event->msg_id);
		if(event->total_data_len > event->data_len)
			MY_LOGW(TAG, "MQTT_CHUNK, TotalDataLen:%d DataLen:%d DataOffset:%d TopicLen:%d", event->total_data_len, event->data_len, event->current_data_offset, event->topic_len);

		MY_LOGI(TAG,"TOPIC=%.*s", event->topic_len, event->topic);
		MY_LOGI(TAG,"DATA=%.*s", event->data_len, event->data);

		if(event->total_data_len > MQTT_INTERNAL_BUFFER_SIZE)//up to 4k payload
		{
			MY_LOGW(TAG,"MSG size:%d > %d", event->total_data_len, MQTT_INTERNAL_BUFFER_SIZE);
			break;
		}
		if(event->total_data_len > event->data_len) //chunked
		{
			if(event->current_data_offset == 0)//start of chunk
			{
				if(mqtt_receive_buffer[mqtt_instance] != NULL)
					free(mqtt_receive_buffer[mqtt_instance]);
				mqtt_receive_buffer[mqtt_instance] = malloc(event->total_data_len);
				if (mqtt_receive_buffer[mqtt_instance] == NULL)
				{
					MY_LOGE(TAG, "Malloc failed");
					statistics.errors++;
					break;
				}
				memset(chunk_topic, 0, sizeof(chunk_topic));
				memcpy(chunk_topic, event->topic, event->topic_len);
				MY_LOGW(TAG, "MQTT_CHUNK, TotalDataLen:%d DataLen:%d DataOffset:%d Topic:%s", event->total_data_len, event->data_len, event->current_data_offset, chunk_topic);
				memset(mqtt_receive_buffer[mqtt_instance], 0, event->total_data_len);
				memcpy(mqtt_receive_buffer[mqtt_instance], event->data, event->data_len);
			}
			else  //next chunk
			{
				memcpy(mqtt_receive_buffer[mqtt_instance] + event->current_data_offset, event->data, event->data_len);
				if(event->current_data_offset + event->data_len == event->total_data_len)  //end of msg
				{
					MY_LOGI(TAG, "MQTT_PARSE CHUNK Topic:%s Msg size:%d", chunk_topic, strlen(mqtt_receive_buffer[mqtt_instance]));
					parse_msg(chunk_topic, strlen(chunk_topic), mqtt_receive_buffer[mqtt_instance], strlen(mqtt_receive_buffer[mqtt_instance]), client);
					if(mqtt_receive_buffer[mqtt_instance] != NULL)
					{
						free(mqtt_receive_buffer[mqtt_instance]);
						mqtt_receive_buffer[mqtt_instance] = NULL;
					}
				}
			}
		}
		else //single chunk
		{
			//MY_LOGI(TAG, "MQTT_PARSE SINGLE");
			parse_msg(event->topic, event->topic_len, event->data, event->data_len, client);
		}

		break;
	case MQTT_EVENT_ERROR:
		status.mqtt[mqtt_instance].state = event->event_id;
		MY_LOGE(TAG, "MQTT%d EVENT_ERROR", mqtt_instance+1);
		statistics.errors++;
		break;
	case MQTT_EVENT_BEFORE_CONNECT:
		MY_LOGI(TAG, "MQTT%d before connect", mqtt_instance+1);
		break;
	default:
		MY_LOGI(TAG, "MQTT%d Other event id:%d", mqtt_instance+1, event->event_id);
		break;
	}
	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	MY_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	mqtt_event_handler_cb(event_data);
}

static int mqtt_app_start(uint8_t instance)
{
	char mqtt_uri[80]; //mqtt uri is in following format: mqtt://10.120.4.160 or mqtts://10.120.4.160 with tls security
	char filename[50];

	if(settings.publish_server[instance].mqtt_server[0] != 0)
	{
		if(settings.publish_server[instance].mqtt_tls > 0)//tls
		{
			snprintf(mqtt_uri, 70, "mqtts://%s", settings.publish_server[instance].mqtt_server);
			if(settings.publish_server[instance].mqtt_cert_file[0] == 0)
			{
				//strcpy(settings.publish_server[instance].mqtt_cert_file, CONFIG_MQTT_CLIENT_CERTIFICATE_FILE);
				MY_LOGE(TAG, "MQTT%d Server Certificate not set", instance);
				status.error_flags |= (ERR_MQTT_INIT_1 + instance);
				status.mqtt[instance].state = 7; //certificate missing
				return -1;
			}

			//allocate memory for certificate
			mqtt_cert[instance] = (char*) malloc(CERT_MEM_SIZE);
			if(mqtt_cert[instance] == NULL)
			{
				MY_LOGE(TAG, "%s mqtt_cert MALLOC Failed", __FUNCTION__);
				return -1;
			}


			sprintf(filename, "/spiffs/%s", settings.publish_server[instance].mqtt_cert_file);
			if(read_file(filename, mqtt_cert[instance], CERT_MEM_SIZE) < 0) //if file does not exist, mqtt_cert will be empty
			{
				MY_LOGE(TAG, "MQTT%d Server certificate not present: %s", instance, filename);
				status.error_flags |= (ERR_MQTT_INIT_1 + instance);
				status.mqtt[instance].state = 7; //certificate missing
				free(mqtt_cert[instance]);
				return -1;
			}


			if(settings.publish_server[instance].mqtt_tls == 2)//read additional certificates for tls mutual authentication
			{
				//client certificate
				if(settings.publish_server[instance].mqtt_client_cert_file[0] == 0)
				{
					//strcpy(settings.publish_server[instance].mqtt_key_file, CONFIG_MQTT_CLIENT_KEY_FILE);
					MY_LOGE(TAG, "MQTT%d  Client cert file not set", instance);
					status.error_flags |= (ERR_MQTT_INIT_1 + instance);
					free(mqtt_cert[instance]);
					return -1;
				}
				//allocate memory for certificate
				mqtt_cli_cert[instance] = (char*) malloc(CERT_MEM_SIZE);
				if(mqtt_cli_cert[instance] == NULL)
				{
					MY_LOGE(TAG, "%s  mqtt_cli_cert MALLOC Failed", __FUNCTION__);
					free(mqtt_cert[instance]);
					return -1;
				}

				//read client certificate from spiffs
				sprintf(filename, "/spiffs/%s", settings.publish_server[instance].mqtt_client_cert_file);
				if(read_file(filename, mqtt_cli_cert[instance], CERT_MEM_SIZE) < 0)
				{
					MY_LOGE(TAG, "MQTT%d Client cert not present: %s", instance, filename);
					status.error_flags |= (ERR_MQTT_INIT_1 + instance);
					status.mqtt[instance].state = 7; //certificate missing
					free(mqtt_cert[instance]);
					free(mqtt_cli_cert[instance]);
					return -1;
				}

				//client key
				if(settings.publish_server[instance].mqtt_key_file[0] == 0)
				{
					//strcpy(settings.publish_server[instance].mqtt_key_file, CONFIG_MQTT_CLIENT_KEY_FILE);
					MY_LOGE(TAG, "MQTT%d Client key file not set", instance);
					status.error_flags |= (ERR_MQTT_INIT_1 + instance);
					free(mqtt_cert[instance]);
					free(mqtt_cli_cert[instance]);
					return -1;
				}
				//allocate memory for client key
				mqtt_key[instance] = (char*) malloc(CERT_MEM_SIZE);
				if(mqtt_key[instance] == NULL)
				{
					MY_LOGE(TAG, "%s mqtt_key MALLOC Failed", __FUNCTION__);
					free(mqtt_cert[instance]);
					free(mqtt_cli_cert[instance]);
					return -1;
				}
				//read client key from spiffs
				sprintf(filename, "/spiffs/%s", settings.publish_server[instance].mqtt_key_file);
				if(read_file(filename, mqtt_key[instance], CERT_MEM_SIZE) < 0)
				{
					MY_LOGE(TAG, "MQTT%d Client key not present: %s", instance, filename);
					status.error_flags |= (ERR_MQTT_INIT_1 + instance);
					status.mqtt[instance].state = 7; //certificate missing
					free(mqtt_cert[instance]);
					free(mqtt_cli_cert[instance]);
					free(mqtt_key[instance]);
					return -1;
				}
			}
		}
		else
			snprintf(mqtt_uri, 70, "mqtt://%s", settings.publish_server[instance].mqtt_server);
	}
	else
	{
		MY_LOGE(TAG, "MQTT%d Broker not set", instance);
		status.error_flags |= (ERR_MQTT_INIT_1 + instance);
		//strcpy(mqtt_uri, CONFIG_BROKER_URL);
		return -1;
	}

	esp_mqtt_client_config_t mqtt_cfg = {
			.uri = mqtt_uri,
			.port = settings.publish_server[instance].mqtt_port,
			//.client_cert_pem = (const char *)mqtt_cert[instance],
			//.client_key_pem = (const char *)mqtt_key[instance],
			.buffer_size = CONFIG_MQTT_BUFFER_SIZE,
			//.client_id = factory_settings.serial_number,
			//.uri = CONFIG_BROKER_URL, //from menuconfig
			//.uri = "mqtt://10.120.4.160",
			//.client_cert_pem = (const char *)client_cert_pem_start,
			//.client_key_pem = (const char *)client_key_pem_start
			//.event_handle = mqtt_event_handler,
			// .user_context = (void *)your_context
	};

#if ENCHELE
	const esp_mqtt_client_config_t mqtt_cfg = {
	        .host = "192.168.1.11", //Raspberry running mosquitto IP
		.port = 8883,
		.transport = MQTT_TRANSPORT_OVER_SSL,
	        .event_handle = mqtt_event_handler,
		//.use_global_ca_store = true,
		.cert_pem = (const char *)server_cert_pem_start, //this is the ca.crt file
	        .client_cert_pem = (const char *)client_cert_pem_start,
	        .client_key_pem = (const char *)client_key_pem_start,
	    };

#endif

	if(settings.publish_server[instance].mqtt_tls)//23.9.2020: set keys only if using tls
	{
		mqtt_cfg.cert_pem = (const char *)mqtt_cert[instance];
	}

	if(settings.publish_server[instance].mqtt_tls == 2)//additional certs for mutual authentication
	{
		mqtt_cfg.client_cert_pem = (const char *)mqtt_cli_cert[instance];
		mqtt_cfg.client_key_pem = (const char *)mqtt_key[instance];
	}

	if(strlen(settings.publish_server[instance].mqtt_username))
		mqtt_cfg.username = settings.publish_server[instance].mqtt_username;

	if(strlen(settings.publish_server[instance].mqtt_password))
			mqtt_cfg.password = settings.publish_server[instance].mqtt_password;

	char client_id[20];
	sprintf(client_id, "%s-%d", factory_settings.serial_number, instance+1); //different client id for instance
	mqtt_cfg.client_id = client_id;

	MY_LOGI(TAG, "MQTT%d Broker address: %s", instance, mqtt_cfg.uri);

	sprintf(mqtt_topic[instance].publish, "%s/%s", settings.publish_server[instance].mqtt_root_topic, settings.publish_server[instance].mqtt_pub_topic);
	sprintf(mqtt_topic[instance].subscribe, "%s/%s", settings.publish_server[instance].mqtt_root_topic, settings.publish_server[instance].mqtt_sub_topic);
	//sprintf(mqtt_topic[instance].info, "%s/info", settings.publish_server[instance].mqtt_root_topic);

	MY_LOGI(TAG, "Publish topic is: %s", mqtt_topic[instance].publish);
	MY_LOGI(TAG, "Subscribe topic is: %s", mqtt_topic[instance].subscribe);
	//MY_LOGI(TAG, "Info topic is: %s", mqtt_topic[instance].info);
	//MY_LOGI(TAG, "Stats topic is: %s", mqtt_topic[instance].stats);

	//esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	mqtt_client[instance] = esp_mqtt_client_init(&mqtt_cfg);
	if(mqtt_client[instance] == NULL)
	{
		MY_LOGE(TAG, "ERROR: esp_mqtt_client_init instance:%d", instance);
		status.error_flags |= (ERR_MQTT_INIT_1 + instance);
		return -1;
	}
	esp_mqtt_client_register_event(mqtt_client[instance], ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client[instance]);
	esp_mqtt_client_start(mqtt_client[instance]);
	//mqtt_ready[instance] = 1;
	status.mqtt[instance].enabled = 1;

	return 0;
}

void mqtt_client_task(void *pvParam)
{
	int ret0 = -1;
	int ret1 = -1;
	//*app_desc = esp_ota_get_app_description();

	MY_LOGI(TAG, "Started");

	//esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	//----- WAIT FOR IP ADDRESS -----
	MY_LOGI(TAG, "waiting for SYSTEM_EVENT_ETH_GOT_IP ...");
	xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, portMAX_DELAY);
	MY_LOGI(TAG, "SYSTEM_EVENT_ETH_GOT_IP !!!!!!!!!");

	if(settings.publish_server[0].push_protocol == MQTT_PROTOCOL)
		ret0 = mqtt_app_start(0);

	if(settings.publish_server[1].push_protocol == MQTT_PROTOCOL)
		ret1 = mqtt_app_start(1);

	if(ret0 < 0 && ret1 < 0)
	{
		MY_LOGW(TAG, "No mqtt client started. Exit");
		vTaskDelete( NULL );
	}

	while(1)
	{
		EventBits_t uxBits;

		vTaskDelay(10 / portTICK_PERIOD_MS);

		uxBits = xEventGroupWaitBits(ethernet_event_group, MQTT_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & MQTT_STOP ) == MQTT_STOP )
		{
			MY_LOGI(TAG, "STOP_MQTT_CLIENT!");
			mqtt_ready[0] = 0;
			mqtt_ready[1] = 0;

			if(mqtt_client[0])
			{
				MY_LOGI(TAG, "STOPPING MQTT 1");
				esp_mqtt_client_disconnect(mqtt_client[0]);
				vTaskDelay(200 / portTICK_PERIOD_MS);
				esp_mqtt_client_stop(mqtt_client[0]);
			}
			if(mqtt_client[1])
			{
				MY_LOGI(TAG, "STOPPING MQTT 2");
				esp_mqtt_client_disconnect(mqtt_client[1]);
				vTaskDelay(200 / portTICK_PERIOD_MS);
				esp_mqtt_client_stop(mqtt_client[1]);
			}

			vTaskDelay(1000 / portTICK_PERIOD_MS);
			vTaskDelete( NULL );
		}

#if 0 //#ifdef DISPLAY_STACK_FREE
		UBaseType_t uxHighWaterMark;

		/* Inspect our own high water mark on entering the task. */
		uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		MY_LOGI(TAG, "FREERTOS STACK: %d, Heap free size: %d\n\r",
				uxHighWaterMark, xPortGetFreeHeapSize());

		MY_LOGI(TAG, "ESP Heap: %d, ESP min Heap: %d", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
#endif  
	}
}

//todo: add import certificate through mqtt
void json_parse_cmd(char* json_string, esp_mqtt_client_handle_t client)
{
	int mqtt_instance = 0; //default is 0
	if(client == mqtt_client[1])
		mqtt_instance = 1;

	cJSON *root = cJSON_Parse(json_string);
	if(root != NULL)
	{
		cJSON *data = cJSON_GetObjectItem(root,"data");

		const cJSON *cmd = NULL;
		cmd = cJSON_GetObjectItemCaseSensitive(data, "cmd");

		if (cJSON_IsString(cmd) && (cmd->valuestring != NULL))
		{
			MY_LOGI(TAG, "CMD:%s", cmd->valuestring);

			/* STATUS Command */
			if(!strcmp(cmd->valuestring, "status"))
			{
				char *read_settings_json = getStatusJSON();
				if(read_settings_json != NULL)
				{
					int ret;
					MY_LOGI(TAG, "Status:\n%s", read_settings_json);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", read_settings_json, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d status publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(read_settings_json);
				}
			}

			/* UPGRADE Command */
			else if(!strcmp(cmd->valuestring, "upgrade_app"))
			{
				const cJSON *uri = NULL;
				const cJSON *cert = NULL;

				uri = cJSON_GetObjectItemCaseSensitive(data, "uri");
				cert = cJSON_GetObjectItemCaseSensitive(data, "cert");

				MY_LOGI(TAG,"Upgrade command:%s", cmd->valuestring);
				if( cJSON_IsString(uri) && (uri->valuestring != NULL))
				{
					MY_LOGI(TAG,"uri \"%s\": ", uri->valuestring);
					strncpy(update_url, uri->valuestring, UPDATE_URL_SIZE); //set update uri
					if( cJSON_IsString(cert) && (cert->valuestring != NULL))
					{
						char tmp[100];
						sprintf(tmp, "/spiffs/%s", cert->valuestring); //get certificate filename from mqtt
						if(read_file(tmp, cert_pem, sizeof(cert_pem)) == ESP_OK)
						{
							sprintf(tmp, "Firmware upgrade Started\nUrl:%s", update_url);
							led_set(LED_RED, LED_BLINK_FAST);
							strcpy(status.app_status, "Upgrading");
							mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "log", tmp, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
							vTaskDelay(1000 / portTICK_PERIOD_MS);

							xEventGroupSetBits(ethernet_event_group, HTTP_STOP); //stop http server to release some memory
							xEventGroupSetBits(ethernet_event_group, MQTT_STOP); //stop mqtt client to release some memory
							xEventGroupSetBits(ethernet_event_group, TCP_SERVER_STOP); //stop tcp server to release some memory
							xEventGroupSetBits(ethernet_event_group, BICOM_TASK_STOP); //stop bicom task
							xEventGroupSetBits(ethernet_event_group, ADC_TASK_STOP); //stop adc task
							xEventGroupSetBits(ethernet_event_group, COUNT_TASK_STOP); //stop counter task
							xEventGroupSetBits(ethernet_event_group, MODBUS_SLAVE_STOP); //stop modbus slave
							/* Ensure to disable any WiFi power save mode, this allows best throughput
							 * and hence timings for overall OTA operation.
							 */
							if(settings.connection_mode == WIFI_MODE)
								esp_wifi_set_ps(WIFI_PS_NONE);

							vTaskDelay(1000 / portTICK_PERIOD_MS);
							xTaskCreate(&simple_ota_example_task, "ota_task", 8192, NULL, 5, NULL);
						}
						else
						{
							MY_LOGE(TAG, "MQTT%d cmd: %s: Certificate: %s not found", mqtt_instance+1, cmd->valuestring, cert->valuestring);
						}
					}
					else
					{
						MY_LOGE(TAG, "MQTT%d cmd :%s: No certificate", mqtt_instance+1, cmd->valuestring);
					}
				}
				else
					MY_LOGW(TAG, "No uri\n");
			} //upgrade_app
			/* REBOOT Command */
			else if(!strcmp(cmd->valuestring, "reboot"))
			{
				MY_LOGI(TAG, "Reboot command received");
				MY_LOGI(TAG, "***********************************\n\
				    		Restarting ...\n\
				    		***********************************");
				reset_device();
			}
			/* LED Command */
			else if(!strcmp(cmd->valuestring, "led"))
			{
				const cJSON *arg = NULL;
				const cJSON *val = NULL;

				arg = cJSON_GetObjectItemCaseSensitive(data, "arg");
				val = cJSON_GetObjectItemCaseSensitive(data, "val");

				MY_LOGI(TAG,"command:%s", cmd->valuestring);
				if( cJSON_IsNumber(arg) && (arg->valueint >= 0) && (arg->valueint < 3) && cJSON_IsString(val) && (val->valuestring != NULL))
				{
					MY_LOGI(TAG,"command:%s arg:%d val:%s", cmd->valuestring, arg->valueint, val->valuestring);
					if(!strncmp(val->valuestring, "on", 2))
						led_set(arg->valueint, LED_ON);
					else if(!strncmp(val->valuestring, "off", 3))
						led_set(arg->valueint, LED_OFF);
				}
			}
			/* MEASUREMENTS and COUNTERS Command */
			else if(!strcmp(cmd->valuestring, "measurements") || !strcmp(cmd->valuestring, "counters"))
			{
				char *read_settings_json = NULL;
				int8_t index = 0; //for mqtt subtopic
				uint8_t ir = 0;

				MY_LOGI(TAG,"command:%s", cmd->valuestring);

				if(cJSON_HasObjectItem(data, "index"))
				{
					const cJSON *cj_index = NULL;
					cj_index = cJSON_GetObjectItemCaseSensitive(data, "index");
					MY_LOGI(TAG,"index:%d", cj_index->valueint);
					if(cJSON_IsNumber(cj_index) && cj_index->valueint < 19)
					{
						index = cj_index->valueint; //index is 1 based, 0 is IR
						int port = RS485_UART_PORT;
						int address = settings.rs485[index - 1].address;
						int number = 0;
						if(index == 0)//IR meter
						{
							ir = 1;
							port = LEFT_IR_UART_PORT;
							address = settings.ir_counter_addr;
						}
						else
							number = getNumberFromIndex(index - 1);

						if(number < 0)
							goto parse_end;
						if(!strcmp(cmd->valuestring, "measurements"))
							read_settings_json = getMeasurementsJSON(port, address, number);
						else if(!strcmp(cmd->valuestring, "counters"))
							read_settings_json = getEnergyCountersJSON(port, address, number);
					}
					else
						goto parse_end;
				}
				else if(cJSON_HasObjectItem(data, "addr"))
				{
					int i_port;
					int address;
					int number = 0;
					const cJSON *addr = NULL;

					addr = cJSON_GetObjectItemCaseSensitive(data, "addr");

					if( cJSON_IsNumber(addr) && (addr->valueint >= 0) && (addr->valueint < 248))
					{
						//index = addr->valueint;
						MY_LOGI(TAG,"command:%s addr:%d", cmd->valuestring, addr->valueint);
						if(addr->valueint == 0)
						{
							ir = 1;
							i_port = LEFT_IR_UART_PORT;//IR port
							address = settings.ir_counter_addr;
						}
						else
						{
							address = addr->valueint;
							index = getIndexFromAddress(address) + 1;
							if(index < 0)
								goto parse_end;
							number = getNumberFromIndex(index - 1);
							if(number < 0)
								goto parse_end;
							i_port = RS485_UART_PORT;
						}

						if(!strcmp(cmd->valuestring, "measurements"))
							read_settings_json = getMeasurementsJSON(i_port, address, number);
						else if(!strcmp(cmd->valuestring, "counters"))
							read_settings_json = getEnergyCountersJSON(i_port, address, number);
					}
					else
						goto parse_end;
				}
				else
					goto parse_end;

				if(read_settings_json != NULL)
				{
					int ret;
					char subtopic[20];

					if(ir == 1)
						sprintf(subtopic, "%s/ir", cmd->valuestring);
					else
						sprintf(subtopic, "%s/rs485/%d", cmd->valuestring, index);

					MY_LOGI(TAG, "%s:\n%s", cmd->valuestring, read_settings_json);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, subtopic, read_settings_json, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d measurements publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(read_settings_json);
				}
			}

			/* BICOM Command */
			else if(!strcmp(cmd->valuestring, "bicom"))
			{
				const cJSON *val = NULL;
				int modbus_addr = 255;
				int is_number = 0; //modbus address or numberred setting
				int i_index = 0;

				val = cJSON_GetObjectItemCaseSensitive(data, "val");

				if(cJSON_HasObjectItem(data, "index"))
				{
					const cJSON *index = NULL;
					index = cJSON_GetObjectItem(data, "index");
					if(cJSON_IsNumber(index) && index->valueint < RS485_DEVICES_NUM + 1)
					{
						i_index = index->valueint;
						if(i_index == 0)//IR bicom
							modbus_addr = 0;
						else
						{
							modbus_addr = settings.rs485[i_index - 1].address; //use bicom settings
							is_number = 1;
						}
					}
					else
						goto parse_end;
				}
				else if(cJSON_HasObjectItem(data, "addr"))
				{
					const cJSON *addr = NULL;
					addr = cJSON_GetObjectItem(data, "addr");
					if( cJSON_IsNumber(addr) && (addr->valueint >= 0) && (addr->valueint < 248) )
						modbus_addr = addr->valueint;
					else goto parse_end;
				}
				MY_LOGI(TAG,"command:%s addr:%d", cmd->valuestring, modbus_addr);
				if( cJSON_IsString(val) && (val->valuestring != NULL))
				{
					MY_LOGI(TAG,"command:%s addr:%d val:%s", cmd->valuestring, modbus_addr, val->valuestring);
					if(!strncmp(val->valuestring, "state", 5))//get bicom state
					{
						if(is_number)
							modbus_addr = i_index - 1;
						char *bicom_status_json = getBicomStatusJSON(modbus_addr, is_number);
						if(bicom_status_json == NULL)
							goto parse_end;
						char subtopic[30];
						//int index = 0;
						if(modbus_addr == 0)
							sprintf(subtopic, "bicom/ir");
						else
						{
							if(is_number == 0)//we need index for subtopic
							{
								i_index = getIndexFromAddress(modbus_addr);
								if(i_index < 0)
									goto parse_end;
								i_index++;//1 based
							}
							sprintf(subtopic, "bicom/rs485/%d", i_index );
						}
						MY_LOGI(TAG, "Bicom %d status:\n%s", i_index, bicom_status_json);
						int ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, subtopic, bicom_status_json, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
						if(ret != ESP_OK)
						{
							MY_LOGE(TAG, "MQTT:%d bicom publish failed :%d", mqtt_instance+1, ret);
							statistics.errors++;
						}
						free(bicom_status_json);
					}//set value
					else if(modbus_addr == 0) //IR bicom
					{
						if(!strncmp(val->valuestring, "on", 2))
							set_ir_bicom_state(1);
						else if(!strncmp(val->valuestring, "off", 3))
							set_ir_bicom_state(0);
					}
					else
					{
						if(!strncmp(val->valuestring, "on", 2))
							set_bicom_485_address_state(modbus_addr, 1);
						else if(!strncmp(val->valuestring, "off", 3))
							set_bicom_485_address_state(modbus_addr, 0);
					}
				}
			}

			/* get bicoms Command */
			else if(!strcmp(cmd->valuestring, "get_bicoms"))
			{
				char *msg = getBicomsJSON();
				if(msg != NULL)
				{
					int ret;
					MY_LOGI(TAG, "Get bicoms len:%d\n%s", strlen(msg), msg);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", msg, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d get_settings publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(msg);
				}
			}
			/* SET SETTINGS Command */
			else if(!strcmp(cmd->valuestring, "set_settings"))
			{
				const cJSON *json_value = NULL;
				int i;
				for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
					json_value = cJSON_GetObjectItemCaseSensitive(data, nvs_settings_array[i].name);
					if(json_value != NULL)
					{
						if(nvs_settings_array[i].type == NVS_STRING && cJSON_IsString(json_value) && json_value->valuestring != NULL)
							strcpy(nvs_settings_array[i].value, json_value->valuestring);
						else if(nvs_settings_array[i].type == NVS_INT32 && cJSON_IsNumber(json_value))
							*(int32_t *)nvs_settings_array[i].value = json_value->valuedouble;
						else if(nvs_settings_array[i].type == NVS_INT16 && cJSON_IsNumber(json_value))
							*(int16_t *)nvs_settings_array[i].value = json_value->valuedouble ;
						else if(nvs_settings_array[i].type == NVS_UINT16 && cJSON_IsNumber(json_value))
							*(uint16_t *)nvs_settings_array[i].value = json_value->valuedouble ;
						else if(nvs_settings_array[i].type == NVS_INT8 && cJSON_IsNumber(json_value))
							*(int8_t *)nvs_settings_array[i].value = json_value->valuedouble ;
						else if(nvs_settings_array[i].type == NVS_UINT8 && cJSON_IsNumber(json_value))
							*(uint8_t *)nvs_settings_array[i].value = json_value->valuedouble ;
						else
							MY_LOGE(TAG, "%s unknown nvs type: %d at %d:%s", __FUNCTION__, nvs_settings_array[i].type, i, nvs_settings_array[i].name);
					}
				}

				err_t err = save_settings_nvs(RESTART);
				if(err != ESP_OK)
				{
					MY_LOGE(TAG, "Save settings Failed:%d", err);
					statistics.errors++;
				}
				else
				{
					MY_LOGI(TAG, "Settings updated");
				}
			}//set_settings
			/* GET SETTINGS Command */
			else if(!strcmp(cmd->valuestring, "get_settings"))
			{
				char *read_settings_json = get_settings_json();
				if(read_settings_json != NULL)
				{
					int ret;
					MY_LOGI(TAG, "Settings len:%d\n%s", strlen(read_settings_json), read_settings_json);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", read_settings_json, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d get_settings publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(read_settings_json);
				}
			}
			/* STATISTICS Command */
			else if(!strcmp(cmd->valuestring, "statistics"))
			{
				char *read_settings_json = getStatisticsJSON();
				if(read_settings_json != NULL)
				{
					int ret;
					MY_LOGI(TAG, "Statistics:\n%s", read_settings_json);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status",read_settings_json, 0, 0, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d statistics publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(read_settings_json);
				}
			}
			/* Status Command */
			else if(!strcmp(cmd->valuestring, "get_status"))
			{
				char *msg = getStatusJSON();
				if(msg != NULL)
				{
					int ret;
					MY_LOGI(TAG, "Status:\n%s", msg);
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status",msg, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d status publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(msg);
				}
			}
			/* NVS format Command */
			else if(!strcmp(cmd->valuestring, "nvs_format"))
			{
				ESP_LOGI(TAG,"Opening Non-Volatile Storage (NVS) handle... ");
				nvs_handle my_handle;
				esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
				if (err != ESP_OK) {
					MY_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
					statistics.errors++;
					// Close
					nvs_close(my_handle);
				}
				else
				{
					ESP_LOGI(TAG,"Done. Clearing NVS and loading back settings");
					clear_all_keys(my_handle);//clear nvs and reboot
				}
			}
#ifdef EEPROM_STORAGE
			/* */
			else if(!strcmp(cmd->valuestring, "get_energy_recorder"))
			{
				const cJSON *recorder = NULL;
				if(cJSON_HasObjectItem(data, "recorder"))
				{
					recorder = cJSON_GetObjectItem(data, "recorder");
					if( cJSON_IsNumber(recorder) && !(factory_settings.hw_ver < 2 && recorder->valueint > 1))
					{
						char *msg = EE_read_energy_day_records_JSON(recorder->valueint - 1);
						if(msg != NULL)
						{
							int ret;
							MY_LOGI(TAG, "get_energy_recorder len:%d\n%s", strlen(msg), msg);
							ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", msg, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
							if(ret != ESP_OK)
							{
								MY_LOGE(TAG, "MQTT:%d get_energy_recorder publish failed :%d", mqtt_instance+1, ret);
								statistics.errors++;
							}
							free(msg);
						}
					}
				}
			}
			/* erase_energy_recorder Command */
			else if(!strcmp(cmd->valuestring, "erase_energy_recorder"))
			{
				const cJSON *recorder = NULL;
				if(cJSON_HasObjectItem(data, "recorder"))
				{
					recorder = cJSON_GetObjectItem(data, "recorder");
					if( cJSON_IsNumber(recorder) && !(factory_settings.hw_ver < 2 && recorder->valueint > 1))
						erase_energy_recorder(recorder->valueint -1);
				}
				else
					MY_LOGW(TAG, "erase_energy_recorder: Missing recorder number");
			}
			else if(!strcmp(cmd->valuestring, "energy_write"))
			{
				cJSON *value_array = NULL;
				cJSON *iterator = NULL;
				const cJSON *type = NULL;
				const cJSON *recorder = NULL;

				value_array = cJSON_GetObjectItem(data, "values");
				if (!cJSON_IsArray(value_array)) {
					goto parse_end;
				}

				type = cJSON_GetObjectItem(data, "type");
				if(type == NULL)
					goto parse_end;

				if(cJSON_HasObjectItem(data, "recorder"))
				{
					recorder = cJSON_GetObjectItem(data, "recorder");
					if( cJSON_IsNumber(recorder) && !(factory_settings.hw_ver < 2 && recorder->valueint > 1))
					{
						int index = 0;
						/* iterate over values */
						cJSON_ArrayForEach(iterator, value_array) {
							if (cJSON_IsNumber(iterator)) {
								//MY_LOGI(TAG, "value: %d", iterator->valueint);
								ee_write_energy_value(index++, iterator->valueint, type->valueint, recorder->valueint - 1);
							}
						}
					}
				}
				else
					MY_LOGW(TAG, "erase_energy_recorder: Missing recorder number");
			}
#endif	//EEPROM_STORAGE
			/* Detect RS485 devices Command */
			else if(!strcmp(cmd->valuestring, "detect_485_devices"))
			{
				int i;
				int ret = 0;
				uint32_t baud_rate_table[] ={1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
				uint16_t stop_bits_table[] ={UART_STOP_BITS_1, UART_STOP_BITS_2};
				const cJSON *baud_rate = NULL;
				const cJSON *stop_bits = NULL;

				baud_rate = cJSON_GetObjectItem(data, "baud_rate");
				if(baud_rate == NULL)
				{
					ESP_LOGW(TAG,"baud_rate missing");
					goto parse_end;
				}

				for(i = 0; i < 8;i++)
				{
					if(baud_rate->valueint == baud_rate_table[i])
					{
						ret = 1;
						break;
					}
				}

				if(ret == 0)
				{
					ESP_LOGW(TAG,"baud_rate not valid: %d", baud_rate->valueint);
					goto parse_end;
				}

				stop_bits = cJSON_GetObjectItem(data, "stop_bits");
				if(stop_bits == NULL)
				{
					ESP_LOGW(TAG,"stop_bits missing");
					goto parse_end;
				}

				set_rs485_params(baud_rate->valueint, stop_bits->valueint);

				ESP_LOGI(TAG,"Detecting RS485 devices at %d, stop bits: %d ... ", baud_rate->valueint, stop_bits->valueint);
				char *msg = search_by_serial_number_JSON(baud_rate->valueint, 0, stop_bits->valueint);
				if(msg != NULL)
				{
					ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", msg, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
					if(ret != ESP_OK)
					{
						MY_LOGE(TAG, "MQTT:%d search_by_serial_number publish failed :%d", mqtt_instance+1, ret);
						statistics.errors++;
					}
					free(msg);
				}
				set_rs485_params(baud_rate_table[settings.rs485_baud_rate], stop_bits_table[settings.rs485_stop_bits]);//restore rs485 settings
			}
			/* modbus read command */
			else if(!strcmp(cmd->valuestring, "modbus_read"))
			{
				const cJSON *modbus_address = NULL;
				const cJSON *serial_number = NULL;
				const cJSON *reg = NULL;
				const cJSON *no_regs = NULL;
				char serial[10];
				int addr;

				MY_LOGI(TAG,"MQTT modbus read");

				if(cJSON_HasObjectItem(data, "modbus_address"))
				{
					modbus_address = cJSON_GetObjectItem(data, "modbus_address");
					addr = modbus_address->valueint;
					serial[0] = 0;
				}
				else if(cJSON_HasObjectItem(data, "serial_number"))
				{
					serial_number = cJSON_GetObjectItem(data, "serial_number");
					strncpy(serial, serial_number->valuestring, 8);
					serial[8] = 0; //terminator
					addr = 0; //addressing by serial number
				}
				else
				{
					ESP_LOGW(TAG,"modbus address or serial number missing");
					goto parse_end;
				}
				reg = cJSON_GetObjectItem(data, "reg");
				if(reg == NULL)
				{
					ESP_LOGW(TAG,"register missing");
					goto parse_end;
				}
				no_regs = cJSON_GetObjectItem(data, "no_regs");
				if(no_regs == NULL)
				{
					ESP_LOGW(TAG,"no_regs missing");
					goto parse_end;
				}

				//MY_LOGI(TAG,"modbus_read addr:%d, reg:%d, no regs :%d serial:%s", addr, reg->valueint, no_regs->valueint, serial);
				read_modbus_mqtt(addr, reg->valueint, no_regs->valueint, mqtt_instance, serial);
			}//modbus_read
			/* modbus modbus_write_single register command */
			else if(!strcmp(cmd->valuestring, "modbus_write_single"))
			{
				const cJSON *modbus_address = NULL;
				const cJSON *serial_number = NULL;
				const cJSON *reg = NULL;
				const cJSON *value = NULL;
				char *string = NULL;
				char serial[10];
				int addr;

				//MY_LOGI(TAG,"MQTT modbus write single reg");

				if(cJSON_HasObjectItem(data, "modbus_address"))
				{
					modbus_address = cJSON_GetObjectItem(data, "modbus_address");
					addr = modbus_address->valueint;
					serial[0] = 0;
				}
				else if(cJSON_HasObjectItem(data, "serial_number"))
				{
					serial_number = cJSON_GetObjectItem(data, "serial_number");
					strncpy(serial, serial_number->valuestring, 8);
					serial[8] = 0; //terminator
					addr = 0; //addressing by serial number
				}
				else
				{
					ESP_LOGW(TAG,"modbus address or serial number missing");
					goto parse_end;
				}
				reg = cJSON_GetObjectItem(data, "reg");
				if(reg == NULL)
				{
					ESP_LOGW(TAG,"register missing");
					goto parse_end;
				}
				value = cJSON_GetObjectItem(data, "value");
				if(value == NULL)
				{
					ESP_LOGW(TAG,"value missing");
					goto parse_end;
				}

				//MY_LOGI(TAG,"write_single_modbus_mqtt addr:%d, reg:%d, val:%x serial:%s", addr, reg->valueint, value->valueint, serial);
				int ret = write_single_modbus_mqtt(addr, reg->valueint, value->valueint, serial);
				//MY_LOGI(TAG,"write_single_modbus_mqtt ret:%d", ret);

				cJSON *json_root = cJSON_CreateObject();

				if(ret >= 0)
					cJSON_AddItemToObject(json_root, "write_single_modbus_mqtt", cJSON_CreateString("OK"));
				else
					cJSON_AddItemToObject(json_root, "write_single_modbus_mqtt", cJSON_CreateString("ERROR"));

				if(addr)
					cJSON_AddItemToObject(json_root, "modbus address", cJSON_CreateNumber(addr));
				else
					cJSON_AddItemToObject(json_root, "serial number", cJSON_CreateString(serial));

				cJSON_AddItemToObject(json_root, "register", cJSON_CreateNumber(reg->valueint));
				cJSON_AddItemToObject(json_root, "value", cJSON_CreateNumber(value->valueint));

				string = cJSON_Print(json_root);
				if (string == NULL)
				{
					MY_LOGE(TAG, "Failed to print status JSON");
					statistics.errors++;
				}

				cJSON_Delete(json_root);

				MY_LOGI(TAG, "%s", string);
				ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", string, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
				if(ret != ESP_OK)
				{
					MY_LOGE(TAG, "MQTT:%d write_single_modbus_mqtt publish failed :%d", mqtt_instance+1, ret);
					statistics.errors++;
				}
				if(string != NULL)
					free(string);

			}//modbus_write_single
			/* modbus write multiple register command */
			else if(!strcmp(cmd->valuestring, "modbus_write_multi"))
			{
				const cJSON *modbus_address = NULL;
				const cJSON *serial_number = NULL;
				const cJSON *reg = NULL;
				const cJSON *value_array = NULL;
				cJSON *values = NULL;
				char write_array[40];
				char serial[10];
				int addr;
				char *string = NULL;
				int i;

				if(cJSON_HasObjectItem(data, "modbus_address"))
				{
					modbus_address = cJSON_GetObjectItem(data, "modbus_address");
					addr = modbus_address->valueint;
					serial[0] = 0;
				}
				else if(cJSON_HasObjectItem(data, "serial_number"))
				{
					serial_number = cJSON_GetObjectItem(data, "serial_number");
					strncpy(serial, serial_number->valuestring, 8);
					serial[8] = 0; //terminator
					addr = 0; //addressing by serial number
				}
				else
				{
					ESP_LOGW(TAG,"modbus address or serial number missing");
					goto parse_end;
				}
				reg = cJSON_GetObjectItem(data, "reg");
				if(reg == NULL)
				{
					ESP_LOGW(TAG,"register missing");
					goto parse_end;
				}
				value_array = cJSON_GetObjectItem(data, "values");
				if (!cJSON_IsArray(value_array)) {
					goto parse_end;
				}

				cJSON *iterator = NULL;
				int size = 0;
				cJSON_ArrayForEach(iterator, value_array) {
					if (cJSON_IsNumber(iterator) && size < 39) {
						//MY_LOGI(TAG, "value: %d", iterator->valueint);
						write_array[size++] = iterator->valueint;
					}
				}

				if(size%2 || size < 2)//odd number of bytes is invalid
				{
					MY_LOGE(TAG, "ODD Number or not enough bytes: %d", size);
					goto parse_end;
				}

				int ret = write_multi_modbus_mqtt(addr, reg->valueint, write_array, size, serial);

				cJSON *json_root = cJSON_CreateObject();

				if(ret == 8)
					cJSON_AddItemToObject(json_root, "write_multi_modbus_mqtt", cJSON_CreateString("OK"));
				else
					cJSON_AddItemToObject(json_root, "write_multi_modbus_mqtt", cJSON_CreateString("ERROR"));

				if(addr)
					cJSON_AddItemToObject(json_root, "modbus address", cJSON_CreateNumber(addr));
				else
					cJSON_AddItemToObject(json_root, "serial number", cJSON_CreateString(serial));

				cJSON_AddItemToObject(json_root, "register", cJSON_CreateNumber(reg->valueint));

				values = cJSON_CreateArray();
				cJSON_AddItemToObject(json_root, "values", values);

				for(i = 0; i < size; i+=2)
				{
					char temp[10];
					sprintf(temp, "0x%04x", (write_array[i]<<8 | write_array[i + 1]));
					cJSON_AddItemToArray(values, cJSON_CreateString(temp));
				}

				string = cJSON_Print(json_root);
				if (string == NULL)
				{
					MY_LOGE(TAG, "Failed to print status JSON");
					statistics.errors++;
				}

				cJSON_Delete(json_root);

				MY_LOGI(TAG, "%s", string);
				ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", string, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
				if(ret != ESP_OK)
				{
					MY_LOGE(TAG, "MQTT:%d write_multi_modbus_mqtt publish failed :%d", mqtt_instance+1, ret);
					statistics.errors++;
				}
				if(string != NULL)
					free(string);

			}//modbus_write_multi
			//write certificate to filesystem
			else if(!strcmp(cmd->valuestring, "write_cert"))
			{
				const cJSON *name = NULL;
				const cJSON *cert = NULL;
				char *string = NULL;
				char cert_filename[32];

				name = cJSON_GetObjectItem(data, "name");
				if(name == NULL)
				{
					ESP_LOGW(TAG,"name missing");
					goto parse_end;
				}
				if(strlen(name->valuestring)>30)
				{
					ESP_LOGW(TAG,"cert name too long");
					goto parse_end;
				}
				cert = cJSON_GetObjectItem(data, "cert");
				if(cert == NULL)
				{
					ESP_LOGW(TAG,"cert missing");
					goto parse_end;
				}
				if(strlen(cert->valuestring)>4096)
				{
					ESP_LOGW(TAG,"cert file too long");
					goto parse_end;
				}
				strcpy(cert_filename, "/spiffs/"); //add root dir
				strcat(cert_filename, name->valuestring);

				int ret = writeFile(cert_filename, cert->valuestring);

				cJSON *json_root = cJSON_CreateObject();

				if(ret >= 0)
					cJSON_AddItemToObject(json_root, "write_cert", cJSON_CreateString("OK"));
				else
					cJSON_AddItemToObject(json_root, "write_cert", cJSON_CreateString("ERROR"));

				cJSON_AddItemToObject(json_root, "name", cJSON_CreateString(name->valuestring));

				string = cJSON_Print(json_root);
				if (string == NULL)
				{
					MY_LOGE(TAG, "Failed to print status JSON");
					statistics.errors++;
				}

				cJSON_Delete(json_root);

				MY_LOGI(TAG, "%s", string);
				ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", string, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
				if(ret != ESP_OK)
				{
					MY_LOGE(TAG, "MQTT:%d write_cert publish failed :%d", mqtt_instance+1, ret);
					statistics.errors++;
				}
				if(string != NULL)
					free(string);
			}
		}
		else
		{
			MY_LOGW(TAG, "MQTT%d No cmd defined", mqtt_instance+1);
		}
	}
	parse_end:
	cJSON_Delete(root);
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *getBicomStatusJSON(int modbus_address, int is_index)
{
	char *string = NULL;
	cJSON *bicom = NULL;
	char result_str[40];
	char measurement_data[128];
	int ret;
	int index = 0;//index from settings

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//create header object
	bicom = cJSON_CreateObject();
	if (bicom == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "bicom", bicom); //bicom object name

	cJSON_AddItemToObject(bicom, "cmd", cJSON_CreateString("get_bicom_status"));
	if(is_index) //modbus address is index in 485 settings
	{
		index = modbus_address;
		modbus_address = settings.rs485[index].address;
		cJSON_AddItemToObject(bicom, "RS485 index", cJSON_CreateNumber(index + 1));
		cJSON_AddItemToObject(bicom, "modbus_addr", cJSON_CreateNumber(modbus_address));
	}
	else
	{
		if(modbus_address > 0)
		{
			index = getIndexFromAddress(modbus_address);
			if(index < 0)
				goto end;
			cJSON_AddItemToObject(bicom, "RS485 index", cJSON_CreateNumber(index + 1));
			cJSON_AddItemToObject(bicom, "modbus_addr", cJSON_CreateNumber(modbus_address));
		}
		else //ir bicom has modbus address 0
			cJSON_AddItemToObject(bicom, "modbus_addr", cJSON_CreateString("N/A"));
	}

	//MY_LOGI(TAG,"addr:%d is_index:%d, index:%d", modbus_address, is_index, index);
	if(modbus_address == 0)//IR bicom
	{
		cJSON_AddItemToObject(bicom, "port", cJSON_CreateString("Left IR"));
		cJSON_AddItemToObject(bicom, "model", cJSON_CreateString("IR Bicom")); //todo: pravi model
		cJSON_AddItemToObject(bicom, "serial_number", cJSON_CreateString(""));
		cJSON_AddItemToObject(bicom, "description", cJSON_CreateString(settings.ir_relay_description));
		cJSON_AddItemToObject(bicom, "location", cJSON_CreateString(""));
	}
	else //RS485 bicom
	{
		cJSON_AddItemToObject(bicom, "port", cJSON_CreateString("RS485"));
		ret = getModelAndSerial(RS485_UART_PORT, modbus_address, result_str, (uint8_t *)measurement_data); //use measurement_data for serial number buffer
		if(ret < 0)
		{
			ESP_LOGE(TAG, "getModelAndSerial failed, uart:%d, addr:%d",RS485_UART_PORT, modbus_address);
			statistics.errors++;
			cJSON_AddItemToObject(bicom, "getModelAndSerial", cJSON_CreateString("error"));
			//goto end;
		}

		cJSON_AddItemToObject(bicom, "model", cJSON_CreateString(result_str));
		cJSON_AddItemToObject(bicom, "serial_number", cJSON_CreateString(measurement_data));

		//description from bicom
		ret = getDescriptionLocationSmall(RS485_UART_PORT, modbus_address, result_str, measurement_data); //use measurement_data for location buffer
		if(ret < 0)
		{
			ESP_LOGE(TAG, "getDescriptionLocationSmall failed, uart:%d, addr:%d",RS485_UART_PORT, modbus_address);
			statistics.errors++;
			cJSON_AddItemToObject(bicom, "getDescriptionLocation", cJSON_CreateString("error"));
			//goto end;
		}

		cJSON_AddItemToObject(bicom, "location", cJSON_CreateString(measurement_data));
		cJSON_AddItemToObject(bicom, "description", cJSON_CreateString(result_str));

		//description from settings
		cJSON_AddItemToObject(bicom, "SG description", cJSON_CreateString(settings.rs485[index].description));
	}

	cJSON_AddItemToObject(bicom, "local_time", cJSON_CreateString(status.local_time));

	if(modbus_address == 0)//IR bicom
		cJSON_AddItemToObject(bicom, "state", cJSON_CreateNumber(get_ir_bicom_state()));

	else //RS485 bicom
		cJSON_AddItemToObject(bicom, "state", cJSON_CreateNumber(get_bicom_485_address_state(modbus_address)));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		ESP_LOGE(TAG, "Failed to print counters JSON");
		statistics.errors++;
	}
	else
		ESP_LOGD(TAG, "%s", string);

	end:
	if (json_root != NULL)
		cJSON_Delete(json_root);

	return string;

}

void parse_msg(char *topic, int topic_len, char *msg, int msg_len, esp_mqtt_client_handle_t client)
{	
	// do something with msg
	MY_LOGI(TAG, "TOPIC=%.*s msg len:%d", topic_len, topic, msg_len);

	if((client == mqtt_client[0] && TOPIC_ENDS_WITH(topic, topic_len, mqtt_topic[0].subscribe)) ||
			(client == mqtt_client[1] && TOPIC_ENDS_WITH(topic, topic_len, mqtt_topic[1].subscribe)))
	{
		MY_LOGI(TAG, "TOPIC=%.*s CMD: %.*s", topic_len, topic, msg_len, msg);
		json_parse_cmd(msg, client);
	}
}

int read_modbus_mqtt(int modbus_address, int reg, int no_regs, int mqtt_instance, char *serial_number)
{
	int functionCode;
	int modbus_reg;
	int command_len = 8;
	int tcp_send_len;
	char *string = NULL;
	int i;
	int offs = 0;
	int ret = 0;

	if(reg < 40000)
	{
		functionCode = 4;
		modbus_reg = reg - 30000;
	}
	else
	{
		functionCode = 3;
		modbus_reg = reg - 40000;
	}

	//MY_LOGI(TAG,"MQTT modbus addr: %d reg: %d no regs: %d serial: %s", modbus_address, reg, no_regs, serial_number);

	char command[20];
	command[0] = modbus_address;

	if(modbus_address == 0) //address by serial number
	{
		int i;
		command[1] = 'L';
		for(i = 0; i < 8; i++)
			command[i + 2] = serial_number[i];

		command_len += 9;//9 additional bytes
		offs = 9;
	}

	command[1 + offs] = (char)functionCode; // Function code for reading registers
	command[2 + offs] = (char)(modbus_reg >> 8); // High word of register
	command[3 + offs] = (char)(modbus_reg & 0xFF); // Low word of register
	command[4 + offs] = (char)(no_regs >> 8); // High word: count of registers to read
	command[5 + offs] = (char)(no_regs & 0xFF); // Low word: count of registers to read

	uint16_t crc = CRC16( (unsigned char*) command, command_len - 2);
	command[command_len - 2] = (char)(crc >> 8);
	command[command_len - 1] = (char)(crc & 0xff);

	//MY_LOGI(TAG,"MQTT modbus addr: %d fc: %d", command[0], command[1]);
	//for(i = 0; i < command_len; i++)
		//MY_LOGI(TAG,"%02x ", command[i]);

	if(modbus_address == settings.sg_modbus_addr) //local modbus
	{
		tcp_send_len = modbus_slave(command, command_len, 0);
	}
#ifdef ENABLE_LEFT_IR_PORT
	else if(modbus_address == settings.ir_counter_addr && settings.left_ir_enabled == 1)
	{
		if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
			status.left_ir_busy = 1;//active
		tcp_send_len = send_receive_modbus_serial(LEFT_IR_PORT_INDEX, command, command_len, MODBUS_RTU);
	}
#endif
#ifdef ENABLE_RS485_PORT
	else //everything else should go to RS485
	{
		tcp_send_len = send_receive_modbus_serial(RS485_PORT_INDEX, command, command_len, MODBUS_RTU);
	}
#endif
	MY_LOGI(TAG,"MQTT modbus packets received: %d bytes", tcp_send_len);

//	if(tcp_send_len > 0)
//	{
//		MY_LOGI(TAG,"MQTT modbus data: %d bytes", tcp_obuf[2]);
//		for(i = 0; i < tcp_send_len; i++)
//			MY_LOGI(TAG,"0x%02x", tcp_obuf[i]);
//	}

	//create JSON
	cJSON *json_root = cJSON_CreateObject();
	cJSON *values;
	if (json_root == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "cmd", cJSON_CreateString("modbus_read"));
	if(modbus_address != 0)
		cJSON_AddItemToObject(json_root, "modbus address", cJSON_CreateNumber(tcp_obuf[0]));
	else
		cJSON_AddItemToObject(json_root, "serial number", cJSON_CreateString(serial_number));
	cJSON_AddItemToObject(json_root, "register", cJSON_CreateNumber(reg));
	cJSON_AddItemToObject(json_root, "number of registers", cJSON_CreateNumber((tcp_obuf[2])/2));

	values = cJSON_CreateArray();
	if (values == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "values", values);

	for(i= 0; i < tcp_obuf[2]; i+=2)
	{
		char temp[10];
		sprintf(temp, "0x%04x", ((tcp_obuf[3 + i])<<8) | tcp_obuf[3 + i + 1]);
		cJSON_AddItemToArray(values, cJSON_CreateString(temp));
	}

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}
	else
	{
		MY_LOGI(TAG, "%s", string);

		ret = mqtt_publish(mqtt_instance, mqtt_topic[mqtt_instance].publish, "status", string, 0, settings.publish_server[mqtt_instance].mqtt_qos, 0);
		if(ret != ESP_OK)
		{
			MY_LOGE(TAG, "MQTT:%d modbus publish failed :%d", mqtt_instance+1, ret);
			statistics.errors++;
		}
	}

	end:
	cJSON_Delete(json_root);

	if(string != NULL)
		free(string);

	return ret;
}

int write_single_modbus_mqtt(int modbus_address, int reg, uint16_t val, char *serial_number)
{
	int ret;
	if(modbus_address == settings.sg_modbus_addr) //local modbus
	{
		int modbus_reg;
		if(reg < 40000)
			return -1;

		modbus_reg = reg - 40000;
		ret = writeModbusReg6(modbus_reg, val);
	}
#ifdef ENABLE_LEFT_IR_PORT
	else if(modbus_address == settings.ir_counter_addr && settings.left_ir_enabled == 1)
	{
		if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
			status.left_ir_busy = 1;//active
		ret = modbus_write(LEFT_IR_PORT_INDEX, modbus_address, reg, val, 1000, serial_number);
		//modbus_write(LEFT_IR_PORT_INDEX, modbus_address, unsigned int reg, unsigned short val, int timeout)
	}
#endif
#ifdef ENABLE_RS485_PORT
	else //everything else should go to RS485
	{
		ret =modbus_write(RS485_PORT_INDEX, modbus_address, reg, val, 1000, serial_number);
	}
#endif
	return ret;
}

int write_multi_modbus_mqtt(int modbus_address, int reg, char write_array[], int byte_size, char serial_number[])
{
	int functionCode = 16;
	int modbus_reg;
	int no_registers = byte_size/2;
	int tcp_send_len = 0;
	int ind = 0;
	int i;

	if(reg < 40000)
	{
		modbus_reg = reg - 30000;
	}
	else
	{
		modbus_reg = reg - 40000;
	}

	char command[50];
	command[0] = modbus_address;

	if(modbus_address == 0) //address by serial number
	{
		int i;
		command[1] = 'L';
		for(i = 0; i < 8; i++)
			command[i + 2] = serial_number[i];

		ind = 9;//9 additional bytes
	}

	command[1 + ind] = functionCode; // Function code for write registers
	command[2 + ind] = (char)(modbus_reg >> 8); // High word of register
	command[3 + ind] = (char)(modbus_reg & 0xFF); // Low word of register
	command[4 + ind] = (char)(no_registers >> 8); // High word: count of registers to write
	command[5 + ind] = (char)(no_registers & 0xFF); // Low word: count of registers to write
	command[6 + ind] = (char)(byte_size); // number of bytes to write

	for(i = 0; i < byte_size; i++)
	{
		command[7 + ind + i] = write_array[i];
	}

	uint16_t crc = CRC16( (unsigned char*) command, 7 + ind + i);
	command[7 + ind + i] = (char)(crc >> 8);
	command[8 + ind + i] = (char)(crc & 0xff);

	int command_len = 9 + ind + i;

	//MY_LOGI(TAG,"MQTT modbus addr: %d len: %d", command[0], command_len);
	//for(i = 0; i < command_len; i++)
		//MY_LOGI(TAG,"%02x ", command[i]);

	if(modbus_address == settings.sg_modbus_addr) //local modbus
	{
		if(reg < 40000)
			return 0;

		writeModbusReg16(modbus_reg, (unsigned short *)write_array, no_registers);
	}
#ifdef ENABLE_LEFT_IR_PORT
	else if(modbus_address == settings.ir_counter_addr && settings.left_ir_enabled == 1)
	{
		if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
			status.left_ir_busy = 1;//active
		tcp_send_len = send_receive_modbus_serial(LEFT_IR_PORT_INDEX, command, command_len, MODBUS_RTU);
		//modbus_write(LEFT_IR_PORT_INDEX, modbus_address, unsigned int reg, unsigned short val, int timeout)
	}
#endif
#ifdef ENABLE_RS485_PORT
	else //everything else should go to RS485
	{
		tcp_send_len = send_receive_modbus_serial(RS485_PORT_INDEX, command, command_len, MODBUS_RTU);
	}
#endif

	//MY_LOGI(TAG,"MQTT write_multi_modbus_mqtt: %d", tcp_send_len);

	return tcp_send_len;
}

#endif //#ifdef START_MQTT_TASK
