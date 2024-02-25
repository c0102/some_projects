#ifndef ENCHELE_LEDC
#define ENCHELE_LEDC
/*
 * Created on Thu Jun 09 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#include <stdbool.h>
void init_led(int led);
void toggle_led_red(bool is_on);
#endif /* ENCHELE_LEDC */
