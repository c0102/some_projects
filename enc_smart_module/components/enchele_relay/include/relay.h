/*
 * Created on Wed Jun 15 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#ifndef ENCHELE_RELAY
#define ENCHELE_RELAY
#include <stdbool.h>
#include "enchele.h"

void init_relay(app_settings_t* settings);
void toggle_relay(bool is_on);
bool get_relay_state();

#endif /* ENCHELE_RELAY */
