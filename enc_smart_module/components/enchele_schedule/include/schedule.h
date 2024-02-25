/*
 * Created on Wed Aug 10 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#ifndef ENCHELE_SCHEDULE
#define ENCHELE_SCHEDULE
#include "enchele.h"


typedef enum{
    MON = 0,
    TUE,
    WED,
    THU,
    FRI,
    SAT,
    SUN
}days_t;

typedef struct
{
    char name[30];
    void* param;

}parameter_t;

typedef struct{
    //uint8_t id;
    parameter_t params[1];
    void (*action)(void* params);

}action_t;


typedef struct{

        uint8_t id;
        uint32_t time;
        action_t action;
        // 0 - monday
        bool repeat[7];

}schedule_t;

void init_scheduler(app_settings_t* settings);

#endif /* ENCHELE_SCHEDULE */
