#include "main.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

/*
The idea is that even if both OTA partitions are somehow corrupted beyond the
ability to boot, you can return to the factory partition and start the update
process fresh from there. In this configuration, the only thing the factory
partition needs to be able to do is to successfully complete an OTA update into
a newer firmware.
*/

/*
it is possible to make a custom partition table that doesn't have a factory partition,
in which case the first OTA app partition becomes the "factory" initial boot partition,
and then each OTA slot is updated with subsequent updates.
This is the recommended option if your SPI flash is too small to dedicate a factory partition.
*/

static const char *TAG = "ota";
//static int reset1s = 1;
//static int reset300s = 300;

//extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
char cert_pem[2048];

//char update_url[] = {"https://10.120.1036:8070/ethernet_demo.bin"};
char update_url[UPDATE_URL_SIZE];

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            MY_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            MY_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            MY_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            MY_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            MY_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            MY_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            MY_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void simple_ota_example_task(void * pvParameter)
{
	esp_timer_handle_t timer;

	MY_LOGI(TAG, "Starting OTA ...");

	status.upgrade.status = UPGRADE_START;
	status.upgrade.start = time(NULL);
	status.upgrade.count++;

	save_i32_key_nvs("upgrade_count", status.upgrade.count);
	save_i32_key_nvs("upgrade_start", status.upgrade.start);
	save_i32_key_nvs("upgrade_end", 0); //0 means not finished
	save_i8_key_nvs("upgrade_status", status.upgrade.status);

	vTaskDelay(5000 / portTICK_PERIOD_MS);

	// reset_device(reset300s); //reset after 5 minutes if upgrade is not finished
	/* Create timer object as a member of app data */
	esp_timer_create_args_t timer_conf = {
			.callback = reset_device,
			.arg = NULL,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "ota_reset_tm"
	};
	esp_err_t err = esp_timer_create(&timer_conf, &timer);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to create timer");
		goto upgrade_end;
	}

	err = esp_timer_start_once(timer, 5*60*1000*1000U); //reset after 5 minutes if upgrade is not finished
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to start timer");
		statistics.errors++;
		goto upgrade_end;
	}

	MY_LOGI(TAG, "OTA timeout timer started");

#ifdef DISPLAY_STACK_FREE
	UBaseType_t uxHighWaterMark;

	/* Inspect our own high water mark on entering the task. */
	uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

	MY_LOGI(TAG, "STACK: %d, Heap free size: %d Heap min: %d",
			uxHighWaterMark, xPortGetFreeHeapSize(), esp_get_minimum_free_heap_size());
#endif

	//strncpy(cert_pem, (char *)server_cert_pem_start, server_cert_pem_end - server_cert_pem_start);

	esp_http_client_config_t config = {
			//.url = CONFIG_FIRMWARE_UPGRADE_URL,
			//.url = "https://smokuc47a.myqnapcloud.com:8070/ethernet_demo.bin",
			.url = update_url,
			.cert_pem = cert_pem, //.cert_pem = (char *)server_cert_pem_start,
			.event_handler = _http_event_handler,
			.keep_alive_enable = true,
			.keep_alive_idle = 10,
			.keep_alive_interval = 10,
			.keep_alive_count = 10,
	};

//#define CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

#if 1
    esp_err_t ret = esp_https_ota(&config);
#else
    esp_err_t ret;
    esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG, "Failed to initialise HTTP connection");
            goto upgrade_end;
        }
        ret = esp_http_client_open(client, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
            esp_http_client_cleanup(client);
            goto upgrade_end;
        }
        ret = esp_http_client_fetch_headers(client);
#endif
    if (ret == ESP_OK) {
    	MY_LOGI(TAG, "\n\r-------------------------------------------------------------\n\r");
    	MY_LOGI(TAG, "\n\r----------------- Firmware upgrade OK. ----------------------\n\r");
    	MY_LOGI(TAG, "\n\r-------------------------------------------------------------\n\r");
    	status.upgrade.status = UPGRADE_OK;

    } else {
    	MY_LOGE(TAG, "Firmware upgrade failed, ret:%d", ret);
        statistics.errors++;
        status.upgrade.status = UPGRADE_FAILED;
    }

upgrade_end:
	status.upgrade.end = time(NULL);
	save_i32_key_nvs("upgrade_end", status.upgrade.end);
	save_i8_key_nvs("upgrade_status", status.upgrade.status);

    reset_device();

    vTaskDelete(NULL);
}

/***********************************************************
 * SPIFFS Upgrade

@brief Task to update spiffs partition
@param pvParameter
*/
#define OTA_BUFF_SIZE 1024
char ota_write_data[OTA_BUFF_SIZE];

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
	status.upgrade.status = UPGRADE_FAILED;
	save_i8_key_nvs("upgrade_status", status.upgrade.status);
	vTaskDelay(1000/portTICK_RATE_MS);

    MY_LOGE(TAG, "Firmware upgrade failed");
    (void)vTaskDelete(NULL);

    reset_device();

    while (1) {
    	;
    }
}

void ota_spiffs_task(void *pvParameter)
{
	esp_err_t err;
	esp_timer_handle_t timer;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	//esp_ota_handle_t update_handle = 0 ;
	//const esp_partition_t *update_partition = NULL;
	esp_partition_t *spiffs_partition=NULL;
	MY_LOGI(TAG, "Starting OTA SPIFFS...");

	//added upgrade statistics and timeout
	status.upgrade.status = UPGRADE_START;
	status.upgrade.start = time(NULL);
	status.upgrade.count++;

	save_i32_key_nvs("upgrade_count", status.upgrade.count);
	save_i32_key_nvs("upgrade_start", status.upgrade.start);
	save_i32_key_nvs("upgrade_end", 0); //0 means not finished
	save_i8_key_nvs("upgrade_status", status.upgrade.status);

	vTaskDelay(5000 / portTICK_PERIOD_MS);
	//reset_device(reset300s); //reset after 5 minutes if upgrade is not finished

	//const esp_partition_t *configured = esp_ota_get_boot_partition();
	//const esp_partition_t *running = esp_ota_get_running_partition();

	/*Update SPIFFS : 1/ First we need to find SPIFFS partition  */

	esp_partition_iterator_t spiffs_partition_iterator=esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS,NULL);
	while(spiffs_partition_iterator !=NULL){
		spiffs_partition = (esp_partition_t *)esp_partition_get(spiffs_partition_iterator);
		printf("main: partition type = %d.\n", spiffs_partition->type);
		printf("main: partition subtype = %d.\n", spiffs_partition->subtype);
		printf("main: partition starting address = 0x%x.\n", spiffs_partition->address);
		printf("main: partition size = 0x%x.\n", spiffs_partition->size);
		printf("main: partition label = %s.\n", spiffs_partition->label);
		printf("main: partition encrypted = %d.\n", spiffs_partition->encrypted);
		printf("\n");
		spiffs_partition_iterator=esp_partition_next(spiffs_partition_iterator);
	}
	vTaskDelay(1000/portTICK_RATE_MS);
	esp_partition_iterator_release(spiffs_partition_iterator);

	//reset after 5 minutes if upgrade is not finished
	/* Create timer object as a member of app data */
	esp_timer_create_args_t timer_conf = {
			.callback = reset_device,
			.arg = NULL,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "ota_reset_tm"
	};
	err = esp_timer_create(&timer_conf, &timer);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to create timer");
		task_fatal_error();
	}

	err = esp_timer_start_once(timer, 5*60*1000*1000U); //reset after 5 minutes if upgrade is not finished
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to start timer");
		task_fatal_error();
	}

	esp_http_client_config_t config = {
			//.url = SPIFFS_SERVER_URL,
			//.cert_pem = (char *)server_cert_pem_start,
			.url = update_url,
			.cert_pem = cert_pem, //.cert_pem = (char *)server_cert_pem_start,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		MY_LOGE(TAG, "Failed to initialise HTTP connection");
		task_fatal_error();
	}
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_cleanup(client);
		task_fatal_error();
	}
	esp_http_client_fetch_headers(client);

	/* 2: Delete SPIFFS Partition  */
	MY_LOGI(TAG, "Erasing SPIFFS partition address:0x%x size:0x%x",spiffs_partition->address,spiffs_partition->size);
	err=esp_partition_erase_range(spiffs_partition, 0, spiffs_partition->size);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Error: esp_partition_erase_range error: %d", err);
	}

	int binary_file_length = 0;
	/*deal with all receive packet*/
	while (1) {
		int data_read = esp_http_client_read(client, ota_write_data, OTA_BUFF_SIZE);
		if (data_read < 0) {
			MY_LOGE(TAG, "Error: SSL data read error");
			http_cleanup(client);
			task_fatal_error();
		} else if (data_read > 0) {
			/* 3 : WRITE SPIFFS PARTITION */
			err = esp_partition_write(spiffs_partition, binary_file_length, (const void *)ota_write_data, data_read);
			if (err != ESP_OK) {
				MY_LOGE(TAG, "Error: esp_partition_write error");
				//continue;
				http_cleanup(client);
				task_fatal_error();
			}

			binary_file_length += data_read;
			//MY_LOGI(TAG, "Written image length %d", binary_file_length);
		} else if (data_read == 0) {
			MY_LOGI(TAG, "Connection closed,all data received");
			break;
		}
	}
	MY_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);
	mqtt_publish(0, mqtt_topic[0].publish, "log", "Firmware upgrade OK.Restarting ...", 0, settings.publish_server[0].mqtt_qos, 0);
	mqtt_publish(1, mqtt_topic[1].publish, "log", "Firmware upgrade OK.Restarting ...", 0, settings.publish_server[1].mqtt_qos, 0);
	MY_LOGI(TAG, "\n\r-------------------------------------------------------------\n\r");
	MY_LOGI(TAG, "\n\r----------------- Firmware upgrade OK. ----------------------\n\r");
	MY_LOGI(TAG, "\n\r-------------------------------------------------------------\n\r");

	status.upgrade.status = UPGRADE_OK;
	status.upgrade.end = time(NULL);
	save_i32_key_nvs("upgrade_end", status.upgrade.end);
	save_i8_key_nvs("upgrade_status", status.upgrade.status);

	reset_device();

	//MY_LOGI(TAG, "Prepare to launch ota APP task or restart!");
	//xTaskCreate(&ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
	vTaskDelete(NULL);
}
