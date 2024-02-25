/*
 * Created on Wed Jun 15 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "enchele.h"

extern SemaphoreHandle_t settings_mutex;
static app_settings_t* app_settings;

/* NOTE(uran): In order to be able to read the pin and know whether the 
*  relay is powered or not we have to set the mode as both input and output
*/  

void init_relay(app_settings_t* settings)
{
    assert(settings);   
    app_settings = settings;
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    gpio_pad_select_gpio(app_settings->RELAY1);
    gpio_set_direction(app_settings->RELAY1, GPIO_MODE_INPUT_OUTPUT);
  xSemaphoreGive(settings_mutex);
}

void toggle_relay(bool is_on)
{
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    gpio_set_level(app_settings->RELAY1, is_on);
  xSemaphoreGive(settings_mutex);
}

bool get_relay_state()
{
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    bool t = gpio_get_level(app_settings->RELAY1);
  xSemaphoreGive(settings_mutex);  
  return t;
}