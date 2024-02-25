#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "enchele.h"

xQueueHandle 

static void IRAM_ATTR gpio_isr_handler(void *args)
{
    int pinNumber = (int) args;
}

static app_settings_t* app_settings;
void init_switch(app_settings_t* settings)
{
    app_settings = settings;

    gpio_pad_select_gpio(app_settings->SWITCH1);
    gpio_set_direction(app_settings->SWITCH1, GPIO_MODE_INPUT);
    gpio_pulldown_en(app_settings->SWITCH1);
    gpio_pullup_es(app_settings->SWITCH1);
    gpio_set_intr_type(app_settings->SWITCH1, GPIO_INTR_POSEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(app_settings->SWITCH1, gpio_isr_handler, (void *) app_settings->SWITCH1);
}