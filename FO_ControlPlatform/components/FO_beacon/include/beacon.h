#ifndef ENCHELE_BEACON_H
#define ENCHELE_BEACON_H




esp_err_t beacon_OFF(void);
esp_err_t beacon_ON(void);
esp_err_t beacon_SLOW(void);
esp_err_t beacon_NORMAL(void);
esp_err_t beacon_FAST(void);
esp_err_t beacon_FAULT_1(void);
esp_err_t beacon_FAULT_2(void);

esp_err_t beacon_init(app_settings_t* settings, outputs_state_t* state);



#endif /* ENCHELE_BEACON_H */
