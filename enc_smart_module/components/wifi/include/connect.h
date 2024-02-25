/*
 * Created on Wed May 10 2022
 * by: Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele
 */


#ifndef ENCHELE_CONNECT
#define ENCHELE_CONNECT

#include "esp_err.h"
#include "esp_wifi.h"

void wifi_init(void);
esp_err_t wifi_connect_sta(const char* ssid, const char* pass, int timeout, int tries);
void wifi_connect_ap(const char *ssid, const char *pass);
void wifi_connect_ap_sta(const char *ssid, const char *pass);
void wifi_disconnect(void);
void wifi_destroy_netif(void);

#endif /* ENCHELE_CONNECT */
