/*
 * Created on Mon Nov 28 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */
#ifndef ENCHELE_VICTRON_UART_H
#define ENCHELE_VICTRON_UART_H
#include "common.h"


#define VED_LINE_SIZE 45		 // Seems to be plenty. VE.Direct protocol could change
#define VED_MAX_LEBEL_SIZE 9	 // Max length of all labels of interest + '\0'. See ved_labels[]
#define VED_MAX_READ_LOOPS 60000 // How many read loops to be considered a read time-out
#define VED_MAX_LINES 25	 	 
#define VED_BUFF_SIZE VED_LINE_SIZE*VED_MAX_LINES

#define VED_BAUD_RATE 19200

typedef enum{
	VE_DUMP = 0,
	VE_SOC,
	VE_VOLTAGE,
	VE_POWER,
	VE_CURRENT,
	VE_ALARM,
	VE_LAST_LABEL,
}VE_DIRECT_DATA;


// const char ved_labels[][VED_MAX_LEBEL_SIZE]  = {
// 		"Dump",	// a string that won't match any label
// 		"SOC",
// 		"V",
// 		"P",
// 		"I",
// 		"Alarm"
// };


esp_err_t vbs_init(app_settings_t* settings, sensor_values_t* sens_val);
#endif /* ENCHELE_VICTRON_UART_H */
