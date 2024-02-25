/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
#include <esp_wifi.h>
#include <esp_event.h>
void ble_provisioning_manager(void);
static esp_err_t ble_prov_manager_start_timeout_timer();
static esp_err_t ble_prov_manager_stop_timeout_timer();
esp_timer_handle_t timer_h;             /*!< Handle to timer */
#endif

#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
extern uint8_t gl_sta_bssid[6];
extern uint8_t gl_sta_ssid[32];
extern int gl_sta_ssid_len;
//extern bool gl_sta_connected;
static int bluetooth_provisioning_started = 0;
#endif

#ifdef CONFIG_BLE_PROV_ENABLED
#include "app_prov.h"
#include <protocomm_security.h>
static int ble_provisioning_started = 0;
static void start_ble_provisioning(void);
#endif //CONFIG_BLE_PROV_ENABLED

static const char *TAG = "wifi_init";
static int s_retry_num = 0;
static wifi_config_t wifi_config;
static int wifi_started = 0;

void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
#ifdef CONFIG_BLE_PROV_ENABLED
	wifi_config_t wifi_config;
#endif

#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
	wifi_config_t wifi_config;
	wifi_mode_t mode;
#endif

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		status.connection = STATUS_CONNECTING;
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < settings.wifi_max_retry) {
			led_set(LED_GREEN, LED_BLINK_FAST); //indicate connecting state
			status.connection = STATUS_CONNECTING;
			esp_wifi_connect();
			xEventGroupClearBits(ethernet_event_group, CONNECTED_BIT | GOT_IP_BIT);
			s_retry_num++;
			MY_LOGI(TAG, "WiFi Disconnected. Retry:%d to connect to the AP", s_retry_num);
		}
		else
		{
			MY_LOGW(TAG, "Wifi connect failed");
			if(settings.wifi_ap_enabled)
				start_soft_ap();
			else
			{
				MY_LOGW(TAG, "Starting Bluetooth provisioning...");
				led_set(LED_ORANGE, LED_BLINK_FAST);
				s_retry_num = 0;
#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
				save_i8_key_nvs(BLE_PROV_NVS_KEY, 1); //set flag for ble_prov_manager
				reset_device(); //reset and start ble prov manager
				//ble_provisioning_manager(); //can't start it here because wifi scan doesnt work if wifi is already connecting
#endif
#ifdef CONFIG_BLE_PROV_ENABLED
				ble_provisioning_started = 1;
				start_ble_provisioning();
#endif //CONFIG_BLE_PROV_ENABLED
#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
				if(bluetooth_provisioning_started == 0)
				{
					bluetooth_init();
					bluetooth_provisioning_started = 1;
				}
#endif //#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
			}//provisioning
		}
	}else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){
		MY_LOGI(TAG, "WiFi Connected.");
		status.connection = STATUS_WIFI_CONNECTED;
	} else if ((event_base == IP_EVENT) && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		char tmp_str[20];
		//MY_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
		s_retry_num = 0;
		//esp_netif_get_ip_info(TCPIP_ADAPTER_IF_STA, &event->ip_info);
		esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &event->ip_info);
		//ESP_ERROR_CHECK(esp_wifi_get_mac(TCPIP_ADAPTER_IF_STA, mac));
		ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, (uint8_t *)&status.mac_addr));
		MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");
		sprintf(status.ip_addr, "" IPSTR "", IP2STR(&event->ip_info.ip)); //add IP address to status
		MY_LOGI(TAG, "WiFi IP: %s", status.ip_addr);
		sprintf(tmp_str, "%02X:%02X:%02X:%02X:%02X:%02X", status.mac_addr[0], status.mac_addr[1], status.mac_addr[2], status.mac_addr[3], status.mac_addr[4], status.mac_addr[5]);
		MY_LOGI(TAG, "MAC: %s", tmp_str);
		ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&event->ip_info.netmask));
		ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&event->ip_info.gw));
		MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");

		esp_netif_dns_info_t dns_info;
		esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &dns_info);
		ESP_LOGI(TAG, "DNS1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
		esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
		ESP_LOGI(TAG, "DNS2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
		MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");
#ifdef CONFIG_BLE_PROV_ENABLED
		if(ble_provisioning_started)//save settings after provisioning
		{
			//esp_wifi_get_config(TCPIP_ADAPTER_IF_STA, &wifi_config);
			if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK) {
			        return;
			    }
			MY_LOGI(TAG, "Provision SSID: %s", wifi_config.sta.ssid);
			strncpy(settings.wifi_ssid, (char *)wifi_config.sta.ssid, 20); //todo 32
			MY_LOGI(TAG, "Provision PASS: %s", wifi_config.sta.password);
			strncpy(settings.wifi_password, (char *)wifi_config.sta.password, 20);//todo 64
			save_settings_nvs(NO_RESTART); //save settings without restart

			ble_provisioning_started = 0;//zurb end of provisioning
		}
#endif //CONFIG_BLE_PROV_ENABLED

#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
		if(bluetooth_provisioning_started)
		{
			esp_blufi_extra_info_t info;

			esp_wifi_get_mode(&mode);

			memset(&info, 0, sizeof(esp_blufi_extra_info_t));
			memcpy(info.sta_bssid, gl_sta_bssid, 6);
			info.sta_bssid_set = true;
			info.sta_ssid = gl_sta_ssid;
			info.sta_ssid_len = gl_sta_ssid_len;
			esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);

			esp_wifi_get_config(TCPIP_ADAPTER_IF_STA, &wifi_config);
			MY_LOGI(TAG, "Provision SSID: %s", wifi_config.sta.ssid);
			strncpy(settings.wifi_ssid, (char *)wifi_config.sta.ssid, 32);
			MY_LOGI(TAG, "Provision PASS: %s", wifi_config.sta.password);
			strncpy(settings.wifi_password, (char *)wifi_config.sta.password, 64);
			save_settings_nvs(NO_RESTART); //save settings without restart

			bluetooth_provisioning_started = 0;//zurb end of provisioning
			//led_set(LED_BLUETOOTH, LED_OFF); //blue led off
			bluetooth_deinit(); //bluetooth is allocating a lot of ram
		}
#endif //#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED

		led_set(LED_GREEN, LED_BLINK_SLOW); //indicate connected state
		xEventGroupSetBits(ethernet_event_group, CONNECTED_BIT);
		xEventGroupSetBits(ethernet_event_group, GOT_IP_BIT);

	}
#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
	else if (event_base == WIFI_PROV_EVENT) {
		switch (event_id) {
		case WIFI_PROV_START:
			MY_LOGI(TAG, "Provisioning started");
			break;
		case WIFI_PROV_CRED_RECV: {
			wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
			ble_prov_manager_stop_timeout_timer();
			MY_LOGI(TAG, "Received Wi-Fi credentials"
					"\n\tSSID     : %s\n\tPassword : %s",
					(const char *) wifi_sta_cfg->ssid,
					(const char *) wifi_sta_cfg->password);

			strncpy(settings.wifi_ssid, (char *)wifi_sta_cfg->ssid, 20); //todo 32
			strncpy(settings.wifi_password, (char *)wifi_sta_cfg->password, 20);//todo 64
			//save_settings_nvs(NO_RESTART); //save settings without restart
			save_string_key_nvs("wifi_ssid", settings.wifi_ssid);
			save_string_key_nvs("wifi_password", settings.wifi_password);
			//update CRC
			status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
			MY_LOGW(TAG,"Save settings OK. Size:%d CRC:%04X", sizeof(settings), status.settings_crc);
			save_u16_key_nvs("settings_crc", status.settings_crc);
			status.nvs_settings_crc = status.settings_crc;
			break;
		}
		case WIFI_PROV_CRED_FAIL: {
			wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
			MY_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
					"\n\tPlease reset to factory and retry provisioning",
					(*reason == WIFI_PROV_STA_AUTH_ERROR) ?
							"Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
			wifi_prov_mgr_deinit();
			break;
		}
		case WIFI_PROV_CRED_SUCCESS:
			MY_LOGI(TAG, "Provisioning successful");
			break;
		case WIFI_PROV_END:
			/* De-initialize manager once provisioning is finished */
			MY_LOGI(TAG, "Provisioning deinit");
			wifi_prov_mgr_deinit();
			break;
		default:
			break;
		}
	}
#endif
#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
		uint16_t apCount = 0;
		esp_wifi_scan_get_ap_num(&apCount);
		if (apCount == 0) {
			BLUFI_INFO("Nothing AP found");
		}
		wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
		if (!ap_list) {
			BLUFI_ERROR("malloc error, ap_list is NULL");
		}
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
		esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
		if (!blufi_ap_list) {
			if (ap_list) {
				free(ap_list);
			}
			BLUFI_ERROR("malloc error, blufi_ap_list is NULL");
		}
		for (int i = 0; i < apCount; ++i)
		{
			blufi_ap_list[i].rssi = ap_list[i].rssi;
			memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
		}
		esp_blufi_send_wifi_list(apCount, blufi_ap_list);
		esp_wifi_scan_stop();
		free(ap_list);
		free(blufi_ap_list);
	}
#endif //#ifdef CONFIG_BLUETOOTH_PROVISIONING_ENABLED
	else if (event_id == WIFI_EVENT_AP_START) {
		ESP_LOGI(TAG, "WIFI AP START");
		led_set(LED_ORANGE, LED_BLINK_SLOW); //AP is ready
		//status.wifiStatusAP = WIFI_AP_START;
	}
	else if (event_id == WIFI_EVENT_AP_STOP ) {
		ESP_LOGI(TAG, "WIFI AP STOP");
		//status.wifiStatusAP = WIFI_AP_STOP;
		status.wifi_AP_started = 0;
	}
	else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
				MAC2STR(event->mac), event->aid);
		//xEventGroupSetBits(ethernet_event_group, CONNECTED_BIT | AP_BIT);
		xEventGroupSetBits(ethernet_event_group, CONNECTED_BIT | GOT_IP_BIT);
		//status.wifiStatusAP = WIFI_AP_CONNECTED;
		//status.wifiStatus = WIFI_STATUS_AP;
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
				MAC2STR(event->mac), event->aid);
		//status.wifiStatusAP = WIFI_AP_DISCONNECTED;
	}
	else {
		MY_LOGI(TAG, "Unknown WiFi event base: %s event: %d", event_base, event_id);
	}
}

void wifi_init()
{
	//wifi_config_t wifi_config = { }; //zero initialize

#ifdef TEST_IMMUNITY
	settings.wifi_max_retry = 60;//2 minutes for immunity test
#endif

	if(settings.wifi_max_retry < 1 || settings.wifi_max_retry > WIFI_RETRY_LIMIT)
	{
		MY_LOGW(TAG, "wifi_max_retry not valid. Using max retry: %d from menuconfig", CONFIG_WIFI_MAXIMUM_RETRY);
		settings.wifi_max_retry = CONFIG_WIFI_MAXIMUM_RETRY;
	}

	if(wifi_started == 0) //init only once
	{
		MY_LOGI(TAG, "WiFi init started");
		led_set(LED_GREEN, LED_BLINK_FAST); //indicate connecting state

		esp_netif_t *esp_netif = esp_netif_create_default_wifi_sta();

#ifdef STATIC_IP
	if(settings.static_ip_enabled)
	{
		ESP_ERROR_CHECK(esp_netif_dhcpc_stop(esp_netif));

		esp_netif_ip_info_t info_t;
		memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
		info_t.ip.addr = esp_ip4addr_aton((const char *)settings.ip);
		info_t.gw.addr = esp_ip4addr_aton((const char *)settings.gateway);
		info_t.netmask.addr = esp_ip4addr_aton((const char *)settings.netmask);
		esp_netif_set_ip_info(esp_netif, &info_t);

		//DNS
		esp_netif_dns_info_t dns_info;
		ip4_addr_t ip;
		ip.addr = esp_ip4addr_aton(settings.dns1);
		ip_addr_set_ip4_u32(&dns_info.ip, ip.addr);
		ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_MAIN, &dns_info));

		ip.addr = esp_ip4addr_aton(settings.dns2);
		ip_addr_set_ip4_u32(&dns_info.ip, ip.addr);
		ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_BACKUP, &dns_info));
	}
#endif //STATIC_IP

		esp_netif_set_hostname(esp_netif, factory_settings.serial_number);

		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));

		/* Register our event handler for Wi-Fi, IP and Provisioning related events */
		ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
		ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED
		ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
#endif
		wifi_started = 1;
	}

#if CONFIG_BLE_PROV_MANAGER_ENABLED
	//zurb 26.2.2020: moved here, because wifi_prov_mgr_init must run every time, so we can free resources later
	/* Configuration for the provisioning manager */
	wifi_prov_mgr_config_t config = {
			/* What is the Provisioning Scheme that we want ?
			 * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
			.scheme = wifi_prov_scheme_ble,
			//        .scheme = wifi_prov_scheme_softap, //zurb

			/* Any default scheme specific event handler that you would
			 * like to choose. Since our example application requires
			 * neither BT nor BLE, we can choose to release the associated
			 * memory once provisioning is complete, or not needed
			 * (in case when device is already provisioned). Choosing
			 * appropriate scheme specific event handler allows the manager
			 * to take care of this automatically. This can be set to
			 * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
			.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
			//        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE //zurb
	};

	/* Initialize provisioning manager with the
	 * configuration parameters set above */
	ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

	int8_t ble_start; //ble provisioning
	read_i8_key_nvs(BLE_PROV_NVS_KEY, &ble_start);
	if(ble_start)
	{
		save_i8_key_nvs(BLE_PROV_NVS_KEY, 0); //remove flag
		status.connection = STATUS_PROVISIONING;
		ble_provisioning_manager();
	}
	else
#endif
	{	//without provisioning

		/* We don't need the manager as device is already provisioned,
		 * so let's release it's resources */
		wifi_prov_mgr_deinit();

		if(settings.wifi_ssid[0] != 0) //use settings if available
		{
			strncpy((char *)wifi_config.sta.ssid, settings.wifi_ssid, 32);
			strncpy((char *)wifi_config.sta.password, settings.wifi_password, 64);
		}
		else //use menuconfig
		{
			strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, 32);
			strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, 64);
		}

		MY_LOGI(TAG, "Connecting to SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
		ESP_ERROR_CHECK(esp_wifi_start() );
	}

	//MY_LOGI(TAG, "Connected to SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
}

int get_wifi_rssi() //todo:vedno enak rssi!!
{
	wifi_ap_record_t wifidata;
	if (esp_wifi_sta_get_ap_info(&wifidata)==0){
		MY_LOGI(TAG,"rssi:%d", wifidata.rssi);
		return wifidata.rssi;
	}
	else return 0;
}

/*********************************************************************************/
// WiFi AP as fallback connectivity
/*********************************************************************************/
static xTimerHandle timerHndlWifiAP = NULL;
static int ap_timer_running = 0;

//reset device after 10 minutes
static void vTimerCallbackWifiAPExpired(xTimerHandle pxTimer)
{
	ap_timer_running = 0; //clear flag
	ESP_LOGI(TAG, "WiFi AP time expired. Resetting...");
	reset_device();
}

void start_wifi_ap_timer()
{
	if(ap_timer_running)
		return;

	if(timerHndlWifiAP == NULL)
	{
		timerHndlWifiAP = xTimerCreate(
				"Wifi_AP", /* name */
				pdMS_TO_TICKS(1000*60*10), /* 10 minutes */
				pdFALSE, /* one shot */
				(void*)0, /* timer ID */
				vTimerCallbackWifiAPExpired); /* callback */
	}

	if (timerHndlWifiAP == NULL)
		ESP_LOGE(TAG, "ERROR creating Wifi AP timer!");
	else
	{
		if (xTimerStart(timerHndlWifiAP, 0)!=pdPASS)
			ESP_LOGE(TAG, "ERROR starting Wifi AP timer!");
		else
		{
			ESP_LOGI(TAG, "Wifi AP timer started!");
			ap_timer_running = 1;
		}
	}
}

void start_soft_ap()
{
//#define AP_SSID		"SG_AP"
#define AP_PASSWORD	"12345678"
#define MAX_STA_CONN	(1)

	esp_err_t err;

	if(status.wifi_AP_started)
	{
		ESP_LOGW(TAG, "wifi AP already started");
		return;
	}

	MY_LOGW(TAG, "Starting AP init");

	esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
	if(wifiAP == NULL)
	{
		ESP_LOGE(TAG, "esp_netif_create_default_wifi_ap");
		return;
	}

	strncpy((char *)wifi_config.ap.ssid, factory_settings.serial_number, strlen(factory_settings.serial_number) + 1);
	strncpy((char *)wifi_config.ap.password, AP_PASSWORD, strlen(AP_PASSWORD) + 1);

	wifi_config.ap.ssid_len = strlen(factory_settings.serial_number);
	wifi_config.ap.max_connection = MAX_STA_CONN;
	wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

	if (strlen((char *)wifi_config.ap.password) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); //AP and STA mode

	err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
	if(err != ESP_OK)
		ESP_LOGE(TAG, "esp_wifi_set_config error:%d", err);

	ESP_LOGI(TAG, "Starting AP. SSID:%s password:%s", wifi_config.ap.ssid, wifi_config.ap.password);
	ESP_ERROR_CHECK(esp_wifi_start() );

	status.wifi_AP_started = 1;
	start_wifi_ap_timer(); //start 10 minutes WiFi AP timer
}

#ifdef CONFIG_BLE_PROV_ENABLED
static void start_ble_provisioning(void)
{
    /* Security version */
    int security = 0;
    /* Proof of possession */
    const protocomm_security_pop_t *pop = NULL;

#ifdef CONFIG_BLE_USE_SEC_1
    security = 1;
#endif

    /* Having proof of possession is optional */
#ifdef CONFIG_BLE_USE_POP
    const static protocomm_security_pop_t app_pop = {
        .data = (uint8_t *) CONFIG_BLE_POP,
        .len = (sizeof(CONFIG_BLE_POP)-1)
    };
    pop = &app_pop;
#endif

    ESP_ERROR_CHECK(app_prov_start_ble_provisioning(security, pop));
}
#endif //#ifdef CONFIG_BLE_PROV_ENABLED

#ifdef CONFIG_BLE_PROV_MANAGER_ENABLED

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void ble_provisioning_manager(void)
{
	led_set(LED_ORANGE, LED_BLINK_FAST);
	ble_prov_manager_start_timeout_timer(); //timer to reset if provisioning is not done in 5 minutes
#if 0
	/* Register our event handler for Wi-Fi, IP and Provisioning related events */
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

	/* Initialize Wi-Fi including netif with default config */
	esp_netif_create_default_wifi_sta();
	//esp_netif_create_default_wifi_ap(); //zurb

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        /* What is the Provisioning Scheme that we want ?
         * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
        .scheme = wifi_prov_scheme_ble,
//        .scheme = wifi_prov_scheme_softap, //zurb

        /* Any default scheme specific event handler that you would
         * like to choose. Since our example application requires
         * neither BT nor BLE, we can choose to release the associated
         * memory once provisioning is complete, or not needed
         * (in case when device is already provisioned). Choosing
         * appropriate scheme specific event handler allows the manager
         * to take care of this automatically. This can be set to
         * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
//        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE //zurb
    };

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
#endif
    /* If device is not yet provisioned start provisioning service */
    if (1) {
        MY_LOGI(TAG, "Starting provisioning");

        /* What is the Device Service Name that we want
         * This translates to :
         *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
         *     - device name when scheme is wifi_prov_scheme_ble
         */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        /* What is the security level that we want (0 or 1):
         *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
         *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
         *          using X25519 key exchange and proof of possession (pop) and AES-CTR
         *          for encryption/decryption of messages.
         */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        /* Do we want a proof-of-possession (ignored if Security 0 is selected):
         *      - this should be a string with length > 0
         *      - NULL if not used
         */
        const char *pop = "abcd1234";

        /* What is the service key (could be NULL)
         * This translates to :
         *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
         *     - simply ignored when scheme is wifi_prov_scheme_ble
         */
        const char *service_key = NULL;

        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = {
        		/* LSB <---------------------------------------
        		 * ---------------------------------------> MSB */
        		0x21, 0x43, 0x65, 0x87, 0x09, 0xba, 0xdc, 0xfe,
				0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        /* Start provisioning service */
        //ESP_ERROR_CHECK(app_prov_start_softap_provisioning(ssid, CONFIG_EXAMPLE_PASS, security, pop));
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

        /* Uncomment the following to wait for the provisioning to finish and then release
         * the resources of the manager. Since in this case de-initialization is triggered
         * by the default event loop handler, we don't need to call the following */
#if 0
        MY_LOGW(TAG, "Provisioning wait");
        wifi_prov_mgr_wait();
        MY_LOGW(TAG, "Provisioning deinit");
        wifi_prov_mgr_deinit();
#endif
    }
}

//timer to stop ble if it is not provisioned in 5 minutes

/* Callback to be invoked by timer */
static void _stop_prov_cb(void * arg)
{
	led_set(LED_RED, LED_ON); //indicate not connected state
	MY_LOGE(TAG, "BLE PROVISIONING TIMEOUT");
	reset_device();
}

static esp_err_t ble_prov_manager_start_timeout_timer()
{
	/* Create timer object as a member of app data */
	esp_timer_create_args_t timer_conf = {
			.callback = _stop_prov_cb,
			.arg = NULL,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "stop_ble_tm"
	};

	esp_err_t err = esp_timer_create(&timer_conf, &timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to create timer");
		return err;
	}

	err = esp_timer_start_once(timer_h, BLE_PROV_MANAGER_TIMEOUT);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to Start timer");
		return err;
	}

	MY_LOGI(TAG, "Provisioning timer started");
	return ESP_OK;
}

static esp_err_t ble_prov_manager_stop_timeout_timer()
{
	esp_err_t err;

	MY_LOGI(TAG, "Stopping ble prov timeout timer");

	err =  esp_timer_stop(timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to stop timer");
		return err;
	}

	err =  esp_timer_delete(timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to delete timer");
		return err;
	}

	return ESP_OK;
}
#endif //CONFIG_BLE_PROV_MANAGER_ENABLED
