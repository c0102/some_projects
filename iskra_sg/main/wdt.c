/*
 * wdt.c
 *
 *  Created on: May 13, 2020
 *      Author: iskra
 */
#include "main.h"

#ifdef EXTERNAL_WATCHDOG_ENABLED
static const char *TAG = "WDT";

static void watchdog_pin_init()
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = (1ULL<<(GPIO_WATCHDOG_PIN));
	//disable pull-down mode
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	//disable pull-up mode
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
}

static void watchdog_refresh()
{
	gpio_set_level(GPIO_WATCHDOG_PIN, 0);
	//MY_LOGI(TAG, "Refresh");
	vTaskDelay(10 / portTICK_PERIOD_MS);
	gpio_set_level(GPIO_WATCHDOG_PIN, 1);
}

static void wdt_task(void *pvParameters)
{
	int timeout;
	MY_LOGI(TAG, "WDT Task started");
	watchdog_pin_init();

	//state 1 after reset: unconditionally refresh WDT
	MY_LOGI(TAG, "WDT State 1: after reset");
	timeout = 5 * 5; //5 seconds
	while(timeout--)
	{
		watchdog_refresh();
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	//state 2 factory reset: unconditionally refresh WDT till the end of factory reset procedure
	MY_LOGI(TAG, "WDT State 2: factory reset");
	while(1)
	{
		EventBits_t uxBits;

		uxBits = xEventGroupWaitBits(ethernet_event_group, FACTORY_RESET_END, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & FACTORY_RESET_END ) == FACTORY_RESET_END )
		{
			MY_LOGI(TAG, "Factory reset end");
			break; //exit state 2
		}

		watchdog_refresh();
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	//state 3 refresh WDT if reset is still pressed
	MY_LOGI(TAG, "WDT State 3: after factory reset");
	while(gpio_get_level(GPIO_FA_RESET) == 0)//refresh WDT if reset pin is still active, because it is a part of factory reset procedute
	{
		watchdog_refresh();
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	//state 4 refresh WDT if reset is not pressed
	MY_LOGI(TAG, "WDT State 4: normal wdt");
	while(1)
	{
		if(gpio_get_level(GPIO_FA_RESET) == 1)
			watchdog_refresh(); //refresh only if reset button is not pressed
		//else
			//led_set(LED_ORANGE, LED_ON); //indicate reset button press

		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}

void start_wdt_task()
{
	//Create a watchdog timer redfresh task
	xTaskCreate(wdt_task, "wdt", 2048, NULL, 16, NULL);
}
#endif

#if 0 //todo: task watchdog
/* Task_Watchdog Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"

#define TWDT_TIMEOUT_S          3
#define TASK_RESET_PERIOD_S     2

/*
 * Macro to check the outputs of TWDT functions and trigger an abort if an
 * incorrect code is returned.
 */
#define CHECK_ERROR_CODE(returned, expected) ({                        \
            if(returned != expected){                                  \
                printf("TWDT ERROR\n");                                \
                abort();                                               \
            }                                                          \
})

static TaskHandle_t task_handles[portNUM_PROCESSORS];

//Callback for user tasks created in app_main()
void reset_task(void *arg)
{
    //Subscribe this task to TWDT, then check if it is subscribed
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);

    while(1){
        //reset the watchdog every 2 seconds
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);  //Comment this line to trigger a TWDT timeout
        vTaskDelay(pdMS_TO_TICKS(TASK_RESET_PERIOD_S * 1000));
    }
}

void app_main(void)
{
    printf("Initialize TWDT\n");
    //Initialize or reinitialize TWDT
    CHECK_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, false), ESP_OK);

    //Subscribe Idle Tasks to TWDT if they were not subscribed at startup
#ifndef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0));
#endif
#ifndef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1));
#endif

    //Create user tasks and add them to watchdog
    for(int i = 0; i < portNUM_PROCESSORS; i++){
        xTaskCreatePinnedToCore(reset_task, "reset task", 1024, NULL, 10, &task_handles[i], i);
    }

    printf("Delay for 10 seconds\n");
    vTaskDelay(pdMS_TO_TICKS(10000));   //Delay for 10 seconds

    printf("Unsubscribing and deleting tasks\n");
    //Delete and unsubscribe Users Tasks from Task Watchdog, then unsubscribe idle task
    for(int i = 0; i < portNUM_PROCESSORS; i++){
        vTaskDelete(task_handles[i]);   //Delete user task first (prevents the resetting of an unsubscribed task)
        CHECK_ERROR_CODE(esp_task_wdt_delete(task_handles[i]), ESP_OK);     //Unsubscribe task from TWDT
        CHECK_ERROR_CODE(esp_task_wdt_status(task_handles[i]), ESP_ERR_NOT_FOUND);  //Confirm task is unsubscribed

        //unsubscribe idle task
        CHECK_ERROR_CODE(esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(i)), ESP_OK);     //Unsubscribe Idle Task from TWDT
        CHECK_ERROR_CODE(esp_task_wdt_status(xTaskGetIdleTaskHandleForCPU(i)), ESP_ERR_NOT_FOUND);      //Confirm Idle task has unsubscribed
    }


    //Deinit TWDT after all tasks have unsubscribed
    CHECK_ERROR_CODE(esp_task_wdt_deinit(), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_ERR_INVALID_STATE);     //Confirm TWDT has been deinitialized

    printf("Complete\n");
}
#endif
