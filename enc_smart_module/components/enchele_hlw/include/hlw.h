/*
 * Created on Fri Jun 24 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */
#ifndef ENCHELE_HLW
#define ENCHELE_HLW

#include "enchele.h"
//#include "_stdint.h"


typedef struct{
        union {
            struct {
              uint8_t state_reg;
              uint8_t check_reg;
              uint8_t v_param[3];
              uint8_t v_reg[3];
              uint8_t c_param[3];
              uint8_t c_reg[3];
              uint8_t p_param[3];
              uint8_t p_reg[3];
              uint8_t data_up_reg;
              uint8_t pf_reg[2];
              uint8_t checksum;
              }separate;
              uint8_t full[24];
        };
    }hlw_raw_t;

typedef struct 
{
    float voltage;
    float current;
    float active_power;
    float apparent_power;
    float power_factor;
    float active_energy;
    float apparent_energy;
}hlw_data_t;


esp_err_t hlw_init(app_settings_t* settings);
hlw_data_t get_hlw_data(void);
#endif /* ENCHELE_HLW */
