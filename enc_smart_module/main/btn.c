/*
 * Created on Fri Jul 22 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "parse.h"
#include "string.h"

#define BTN 9

extern xSemaphoreHandle settings_mutex;
static xSemaphoreHandle btn_sem;
static app_settings_t* app_settings;

static void IRAM_ATTR on_btn_pushed(void *args)
{
  xSemaphoreGiveFromISR(btn_sem, NULL);
}

static void btn_push_task(void *params)
{
  char time[64];
  while (true)
  {
    xSemaphoreTake(btn_sem, portMAX_DELAY);
    
    if(gpio_get_level(BTN)){
        cJSON *payload = cJSON_CreateObject();
      xSemaphoreTake(settings_mutex, portMAX_DELAY);
        cJSON_AddStringToObject(payload,"id", app_settings->thingname);
      xSemaphoreGive(settings_mutex);
        cJSON_AddStringToObject(payload,"cmd", "change_setting");
        cJSON_AddStringToObject(payload,"setting", "thingname");
        cJSON_AddStringToObject(payload,"value", "new_thingname");
        char *message = cJSON_Print(payload);
        char out_message[CONFIG_MESSAGE_LENGTH];

        parse_message(out_message, message, strnlen(message, CONFIG_MESSAGE_LENGTH));
        cJSON_Delete(payload);
        free(message);
        memset(time, 0, 64);
        get_current_time(time);
    }
    



    
  }
}

void init_btn(app_settings_t* settings)
{
  app_settings = settings;

  btn_sem = xSemaphoreCreateBinary();
  xTaskCreate(btn_push_task, "btn_push_task", 1024*2, NULL, 5, NULL);
  gpio_pad_select_gpio(BTN);
  gpio_set_direction(BTN, GPIO_MODE_INPUT);
  gpio_set_intr_type(BTN, GPIO_INTR_ANYEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BTN, on_btn_pushed, NULL);
}