/*
 * Created on Wed Aug 10 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "enchele.h"

app_settings_t* app_settings;
extern xSemaphoreHandle settings_mutex;

static schedule_t schedules[10];

void schedule_task(void* param){

    while(1){

        vTaskDelay(pdMS_TO_TICKS(10));
    }

}



void init_scheduler(app_settings_t* settings){

    app_settings = settings;
    xTaskCreate(schedule_task,"schedule_task",1024*5, NULL, 10, NULL);
}