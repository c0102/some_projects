#ifndef ENCHELE_PARSE
#define ENCHELE_PARSE
/*
 * Created on Wed May 10 2022
 * by: Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "enchele.h"

typedef struct {
    char body[CONFIG_MESSAGE_LENGTH+1];
    QueueHandle_t resp_queue;
}message_t;

void init_parser(const int task_size, app_settings_t* settings);
void message_parse(void* params);
esp_err_t parse_message(char* out_message, char* in_message, size_t len);

#endif /* ENCHELE_PARSE */
