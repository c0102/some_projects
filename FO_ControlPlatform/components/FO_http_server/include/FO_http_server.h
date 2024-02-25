/*
 * Created on Wed Nov 28 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */

#ifndef ENCHELE_FO_HTTP_SERVER_H
#define ENCHELE_FO_HTTP_SERVER_H

void send_data_ws();
esp_err_t send_ws_message(char *message);
esp_err_t init_server(app_settings_t* settings);

#endif /* ENCHELE_FO_HTTP_SERVER_H */
