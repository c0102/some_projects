#include "main.h"

#ifdef ENABLE_ETHERNET

#define PIN_PHY_POWER CONFIG_PHY_POWER_PIN
#define PIN_SMI_MDC CONFIG_PHY_SMI_MDC_PIN
#define PIN_SMI_MDIO CONFIG_PHY_SMI_MDIO_PIN

static esp_err_t eth_start_timeout_timer();
static esp_err_t eth_stop_timeout_timer();

static const char *TAG = "ethernet_init";
static esp_eth_handle_t s_eth_handle = NULL;
static esp_timer_handle_t eth_timer_h;             /*!< Handle to timer */

#ifdef CONFIG_PHY_USE_POWER_PIN
/**
 * @brief re-define power enable func for phy
 *
 * @param enable true to enable, false to disable
 *
 * @note This function replaces the default PHY power on/off function.
 * If this GPIO is not connected on your device (and PHY is always powered),
 * you can use the default PHY-specific power on/off function.
 */
static void phy_device_power_enable_via_gpio(bool enable)
{
    assert(DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable);

    if (!enable) {
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(false);
    }

    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER, GPIO_MODE_OUTPUT);
    if (enable == true) {
        gpio_set_level(PIN_PHY_POWER, 1);
        ESP_LOGI(TAG, "Power On Ethernet PHY");
    } else {
        gpio_set_level(PIN_PHY_POWER, 0);
        ESP_LOGI(TAG, "Power Off Ethernet PHY");
    }

    vTaskDelay(1); // Allow the power up/down to take effect, min 300us

    if (enable) {
        /* call the default PHY-specific power on function */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(true);
    }
}
#endif

/**
 * @brief gpio specific init
 *
 * @note RMII data pins are fixed in esp32:
 * TXD0 <=> GPIO19
 * TXD1 <=> GPIO22
 * TX_EN <=> GPIO21
 * RXD0 <=> GPIO25
 * RXD1 <=> GPIO26
 * CLK <=> GPIO0
 *
 */
#if 0 //1.7.2019: esp-idf changed ethernet handling
static void eth_gpio_config_rmii(void)
{
    phy_rmii_configure_data_interface_pins();
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}
#endif

/** Event handler for Ethernet events */
static void eth_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
	//uint8_t mac_addr[6] = {0};
	char mac_address[20];
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, status.mac_addr);
        sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X",
        		status.mac_addr[0], status.mac_addr[1], status.mac_addr[2], status.mac_addr[3], status.mac_addr[4], status.mac_addr[5]);
        ESP_LOGI(TAG, "Ethernet HW Addr %s",mac_address);
        xEventGroupSetBits(ethernet_event_group, CONNECTED_BIT);
        status.connection = STATUS_ETHERNET_CONNECTED;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet Link Down");
        led_set(LED_GREEN, LED_BLINK_FAST); //indicate connecting state
        status.connection = STATUS_CONNECTING;
		xEventGroupClearBits(ethernet_event_group, CONNECTED_BIT | GOT_IP_BIT);
		eth_start_timeout_timer(); //reset if link doesn't come back in 2 minutes
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        status.connection = STATUS_CONNECTING;
        led_set(LED_GREEN, LED_BLINK_FAST); //indicate connecting state
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGW(TAG, "Ethernet Stopped");
		xEventGroupClearBits(ethernet_event_group, CONNECTED_BIT | GOT_IP_BIT);
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    //const tcpip_adapter_ip_info_t* ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    sprintf(status.ip_addr, "" IPSTR "", IP2STR(&ip_info->ip)); //add IP address to status
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");

    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    ESP_LOGI(TAG, "DNS1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
    ESP_LOGI(TAG, "DNS2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    MY_LOGI(TAG, "~~~~~~~~~~~~~~~~~~~~~~");

    eth_stop_timeout_timer(); //stop timer
	xEventGroupSetBits(ethernet_event_group, GOT_IP_BIT); //notify tasks which are waiting for IP address that we have it
	led_set(LED_GREEN, LED_BLINK_SLOW); //indicate connected state
}

void ethernet_init()
{
	//Ethernet init
	led_set(LED_GREEN, LED_BLINK_FAST); //indicate connecting state

#if CONFIG_BLE_PROV_MANAGER_ENABLED //bluetooth heap memory release workaround
	//zurb 26.2.2020: wifi_prov_mgr_init must run every time, so we can free resources
	/* Configuration for the provisioning manager */
	wifi_prov_mgr_config_t bl_config = {
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
	ESP_ERROR_CHECK(wifi_prov_mgr_init(bl_config));

	/* We don't need the manager as device is already provisioned,
	 * so let's release it's resources */
	wifi_prov_mgr_deinit();
#endif //workaround

	esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
	esp_netif_t* eth_netif = esp_netif_new(&cfg);

#ifdef STATIC_IP
	if(settings.static_ip_enabled)
	{
		ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));

		//char* ip= "10.96.2.123";
		//char* gateway = "10.96.0.1";
		//char* netmask = "255.255.255.0";
		esp_netif_ip_info_t info_t;
		memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
		info_t.ip.addr = esp_ip4addr_aton((const char *)settings.ip);
		info_t.gw.addr = esp_ip4addr_aton((const char *)settings.gateway);
		info_t.netmask.addr = esp_ip4addr_aton((const char *)settings.netmask);
		esp_netif_set_ip_info(eth_netif, &info_t);

		//DNS
		esp_netif_dns_info_t dns_info;
		ip4_addr_t ip;
		ip.addr = esp_ip4addr_aton(settings.dns1);
		ip_addr_set_ip4_u32(&dns_info.ip, ip.addr);
		ESP_ERROR_CHECK(esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &dns_info));

		ip.addr = esp_ip4addr_aton(settings.dns2);
		ip_addr_set_ip4_u32(&dns_info.ip, ip.addr);
		ESP_ERROR_CHECK(esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_BACKUP, &dns_info));
	}
#endif //STATIC_IP

	ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));

	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

#if CONFIG_ETH_USE_ESP32_EMAC
	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

#if CONFIG_PHY_IP101
	esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_PHY_RTL8201
	esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_PHY_LAN8720
	phy_config.phy_addr = CONFIG_PHY_ADDRESS; //use phy address from menuconfig
	esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_PHY_DP83848
	esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	//vTaskDelay(2000 / portTICK_PERIOD_MS); //wait till phy is ready
	int retry = 10;
	int ret;
	while(retry--)
	{
		ESP_LOGI(TAG, "esp_eth_driver_install");
		ret = esp_eth_driver_install(&config, &s_eth_handle);
		if(ret == ESP_OK) //ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
			break;

		vTaskDelay(1000 / portTICK_PERIOD_MS); //wait till phy is ready
	}
	if(ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Eth PHY init failed. Restarting");
		reset_device();
	}
#endif 	//CONFIG_ETH_USE_ESP32_EMAC

	//ESP_ERROR_CHECK(esp_netif_attach(eth_netif, s_eth_handle));
	ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

	esp_netif_set_hostname(eth_netif, factory_settings.serial_number);

	/* start Ethernet driver state machine */
	ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

	eth_start_timeout_timer();
}

//timer to restart board if ethernet is not connected in 2 minutes
#define ETH_CONNECT_TIMEOUT (2*60*1000*1000U) //2 minutes

/* Callback to be invoked by timer */
static void _stop_eth_cb(void * arg)
{
	led_set(LED_RED, LED_ON); //indicate not connected state
	MY_LOGE(TAG, "Ethernet Connection TIMEOUT");
	if(settings.wifi_ap_enabled)
	{
		eth_stop_timeout_timer();
		wifi_init();//start wifi and AP
	}
	else
		reset_device();
}

static esp_err_t eth_start_timeout_timer()
{
	/* Create timer object as a member of app data */
	esp_timer_create_args_t timer_conf = {
			.callback = _stop_eth_cb,
			.arg = NULL,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "stop_eth_tm"
	};

	esp_err_t err = esp_timer_create(&timer_conf, &eth_timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to create timer");
		return err;
	}

	err = esp_timer_start_once(eth_timer_h, ETH_CONNECT_TIMEOUT);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to Start timer");
		return err;
	}

	MY_LOGI(TAG, "Ethernet connect timer started");
	return ESP_OK;
}

static esp_err_t eth_stop_timeout_timer()
{
	esp_err_t err;

	MY_LOGI(TAG, "Stopping Ethernet timer");

	err =  esp_timer_stop(eth_timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to stop timer");
		return err;
	}

	err =  esp_timer_delete(eth_timer_h);
	if (err != ESP_OK) {
		MY_LOGE(TAG, "Failed to delete timer");
		return err;
	}

	return ESP_OK;
}

#endif //ENABLE_ETHERNET
