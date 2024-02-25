/*
 * Created on Thu Jun 16 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#ifndef ENCHELE_MQTT
#define ENCHELE_MQTT

#include "enchele.h"

esp_err_t send_message_mqtt_with_topic(char* data, char* topic);
esp_err_t send_message_mqtt(char* data);
void mqtt_init(app_settings_t* settings);

#endif /* ENCHELE_MQTT */
