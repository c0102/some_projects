/*
 * Created on Thu Nov 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */

#ifndef ENCHELE_SHT40_FRONT_H
#define ENCHELE_SHT40_FRONT_H

#include "common.h"

esp_err_t sht40_init(app_settings_t* settings, sensor_values_t* sensor_val);
esp_err_t sht30_init(app_settings_t* settings, sensor_values_t* sensor_val);
esp_err_t sht40_get_values(float* temperature, float* humidity);

#endif /* ENCHELE_SHT40_FRONT_H */
