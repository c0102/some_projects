/*
 * Created on Fri Jul 22 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "common.h"
#include "beacon.h"

// #include "cJSON.h"
// #include "parse.h"



#define BTN 0

static const char* TAG = "BTN";

static xSemaphoreHandle btn_sem;
static app_settings_t* app_settings;

static void IRAM_ATTR on_btn_pushed(void *args)
{
  xSemaphoreGiveFromISR(btn_sem, NULL);
}

static void btn_push_task(void *params)
{
   static beacon_mode_t mode = BLINK_SLOW;
  //char time[64];
  while (true)
  {
   
    xSemaphoreTake(btn_sem, portMAX_DELAY);
    if(gpio_get_level(BTN)){
      switch(mode){
        case BEACON_OFF:
            ESP_LOGI(TAG,"switching to BEACON_OFF");
            beacon_OFF();
            mode = BEACON_ON; 
            break;
        case BEACON_ON:
            ESP_LOGI(TAG,"switching to BEACON_ON");
            beacon_ON();
            mode = BLINK_SLOW;
            break;
        case BLINK_SLOW:
            ESP_LOGI(TAG,"switching to BLINK_SLOW");
            beacon_SLOW();
            mode = BLINK_NORMAL;
            break;
        case BLINK_NORMAL:
            ESP_LOGI(TAG,"switching to BLINK_NORMAL");
            beacon_SLOW();
            mode = BLINK_FAST;
            break;
        case BLINK_FAST:
            ESP_LOGI(TAG,"switching to BLINK_FAST");
            beacon_SLOW();
            mode = BEACON_OFF;
            break;
        case BLINK_FAULT_1:
        case BLINK_FAULT_2:
        default:
            mode = BEACON_OFF;
        }
    }

  }
}

void init_btn(app_settings_t* settings)
{
  app_settings = settings;

  btn_sem = xSemaphoreCreateBinary();
#ifdef CONFIG_IDF_TARGET_ESP32
  xTaskCreatePinnedToCore(btn_push_task, "btn_push_task", 1024*5, NULL, 5, NULL, 1);
#else
  xTaskCreate(btn_push_task, "btn_push_task", 1024*5, NULL, 5, NULL);
#endif
  gpio_pad_select_gpio(BTN);
  gpio_set_direction(BTN, GPIO_MODE_INPUT);
  gpio_set_intr_type(BTN, GPIO_INTR_ANYEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BTN, on_btn_pushed, NULL);
}