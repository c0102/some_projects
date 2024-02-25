/*
 * energy_meter.c
 *
 *  Created on: Dec 6, 2019
 *      Author: iskra
 */

#include "main.h"
#include <math.h>

const char *counter_units[] = { "", "Wh", "varh", "VAh"};
const char *counter_phase[] = { "Total", "Phase 1", "Phase 2", "Phase 3"};
const char *counter_tariff[] = { "1,2", "1", "2", "1,2"};

float power_buf[100];
static const char * TAG = "energy meter";
counter_config_t counterConfig[20]; //IE has 4 non resetable and up to 16 resetable counters

char *getMeasurementsJSON(int USARTx, int modbus_address, int number)
{

	char *string = NULL;
	cJSON *header = NULL;
	cJSON *measurements = NULL;
	size_t i;
	char result_str[40];
	char measurement_data[64];
	char value_name[15];
	char model[17];
	char str_value[50];
	int ret;
	int number_of_UI = 3;
	int number_of_P = 4;

	//MY_LOGI(TAG, "USARTx:%d modbus_address:%d number:%d", USARTx, modbus_address, number);

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//create header object
	header = cJSON_CreateObject();
	if (header == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "header", header); //header name
	cJSON_AddItemToObject(header, "cmd", cJSON_CreateString("get_measurements"));
	//cJSON_AddItemToObject(header, "device number", cJSON_CreateNumber(number + 1));
	if(USARTx == RS485_UART_PORT)
	{
		cJSON_AddItemToObject(header, "RS485 index", cJSON_CreateNumber(status.modbus_device[number].index + 1));
		cJSON_AddItemToObject(header, "port", cJSON_CreateString("RS485"));
	}
	else if(USARTx == LEFT_IR_UART_PORT)
		cJSON_AddItemToObject(header, "port", cJSON_CreateString("Left IR"));

	cJSON_AddItemToObject(header, "modbus_addr", cJSON_CreateNumber(modbus_address));

	ret = getModelAndSerial(USARTx, modbus_address, model, (uint8_t *)measurement_data); //use measurement_data for serial number buffer
	if(ret < 0)
	{
		MY_LOGE(TAG, "getModelAndSerial failed, uart:%d, addr:%d",USARTx, modbus_address);
		cJSON_AddItemToObject(header, "getModelAndSerial", cJSON_CreateString("error"));
		statistics.errors++;
		goto end;
	}

	//MY_LOGI(TAG, "Model:%s serial:%s", model, measurement_data);
	cJSON_AddItemToObject(header, "model", cJSON_CreateString(model));
	cJSON_AddItemToObject(header, "serial_number", cJSON_CreateString(measurement_data));

	if(strncmp(model, "WM1", 3) == 0 || strncmp(model, "IE1", 3) == 0 || strncmp(model, "7M.24", 5) == 0)
	{
		number_of_UI = 1;
		number_of_P = 1;
	}

	cJSON_AddItemToObject(header, "phases", cJSON_CreateNumber(number_of_UI));

	//description and location from instrument
	if(strncmp(model, "WM1", 3) == 0)
		ret = getDescriptionLocationSmall(USARTx, modbus_address, result_str, measurement_data); //use measurement_data for location buffer
	else
		ret = getDescriptionLocation(USARTx, modbus_address, result_str, measurement_data); //use measurement_data for location buffer

	if(ret < 0)
	{
		MY_LOGE(TAG, "getDescriptionLocation failed, uart:%d, addr:%d",USARTx, modbus_address);
		statistics.errors++;
		cJSON_AddItemToObject(header, "getDescriptionLocation", cJSON_CreateString("error"));
		//goto end;
	}

	cJSON_AddItemToObject(header, "location", cJSON_CreateString(measurement_data));
	cJSON_AddItemToObject(header, "description", cJSON_CreateString(result_str));

	//description from settings
	if(USARTx == RS485_UART_PORT)
		cJSON_AddItemToObject(header, "SG description", cJSON_CreateString(settings.rs485[status.modbus_device[number].index].description));
	else if(USARTx == LEFT_IR_UART_PORT)
		cJSON_AddItemToObject(header, "SG description", cJSON_CreateString(settings.left_ir_desc));

	cJSON_AddItemToObject(header, "local_time", cJSON_CreateString(status.local_time));

	//create counters object
	measurements = cJSON_CreateObject();
	if (measurements == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "measurements", measurements);

	//add measurements values
	memset(measurement_data, 0, 64);
	ret = read_modbus_registers(USARTx, modbus_address, 30101, 25, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30101");
		statistics.errors++;
		goto end;
	}
	//30101
//	if(detected_left_ir_model_type == WM34 || detected_left_ir_model_type == WM36)
//		valid_measurements = getModbusValue16(measurement_data, 0);//wm3 has invalid measure bits
//	else
//		valid_measurements = 0; //all measurements are valid

	//frequency 30105 -30106
	result_str[0] = '\0';
	convertModbusT5T6((uint8_t *)measurement_data, 4, 2, 1, result_str);
	cJSON_AddItemToObject(measurements, "frequency", cJSON_CreateString(result_str));

	//U1,U2,U3
	for(i = 0; i < number_of_UI; i++)
	{
		//if((valid_measurements & (1<<i)) == 0)    //dont check valid at voltages
		result_str[0] = '\0';
		convertModbusT5T6((uint8_t *)measurement_data, 6+2*i, 3, 0, result_str);
		//sprintf(tmp_str, "\"U%d\":\"%sV\",", i+1, result_str);
		sprintf(value_name, "U%d", i+1);
		sprintf(str_value, "%sV", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
	}

	memset(measurement_data, 0, 64);
	ret = read_modbus_registers(USARTx, modbus_address, 30126, 22, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30126");
		statistics.errors++;
		goto end;
	}

	//I1,I2,I3
	for(i = 0; i < number_of_UI; i++)
	{
		//if((valid_measurements & (1<<i)) == 0)
		result_str[0] = '\0';
		convertModbusT5T6((uint8_t *)measurement_data, +2*i, 3, 1, result_str);
		//else
		//strcpy(result_str, " -- ");

		//sprintf(tmp_str, "\"I%d\":\"%sA\",", i+1, result_str);
		sprintf(value_name, "I%d", i+1);
		sprintf(str_value, "%s A", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
	}

	//Pt,P1,P2,P3
	for(i = 0; i < number_of_P; i++)
	{
		//if(i == 0 || (((valid_measurements & (1<<(i-1))) == 0)))
		result_str[0] = '\0';
		convertModbusT5T6((uint8_t *)measurement_data, 14+2*i, 4, 1, result_str);
		//else
		//strcpy(result_str, " -- ");

		//sprintf(tmp_str, "\"P%d\":\"%sW\",", i, result_str);
		sprintf(value_name, "P%d", i);
		sprintf(str_value, "%s W", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));


		if(number_of_P == 1) //copy P0 to P1 because 1 phase counter has only P0
		{
			//sprintf(tmp_str, "\"P%d\":\"%sW\",", i+1, result_str);
			sprintf(value_name, "P%d", i+1);
			sprintf(str_value, "%s W", result_str);
			cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
		}
	}

	memset(measurement_data, 0, 64);
	ret = read_modbus_registers(USARTx, modbus_address, 30148, 16, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30148");
		statistics.errors++;
		goto end;
	}

	//Qt,Q1,Q2,Q3
	for(i = 0; i < number_of_P; i++)
	{
		//if(i == 0 || (((valid_measurements & (1<<(i-1))) == 0)))
		result_str[0] = '\0';
		convertModbusT5T6((uint8_t *)measurement_data, 0+2*i, 4, 1, result_str);
		//else
		//strcpy(result_str, " -- ");

		//sprintf(tmp_str, "\"Q%d\":\"%svar\",", i, result_str);
		sprintf(value_name, "Q%d", i);
		sprintf(str_value, "%s var", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));

		if(number_of_P == 1) //copy Q0 to Q1 because 1 phase counter has only P0
		{
			//sprintf(tmp_str, "\"Q%d\":\"%svar\",", i+1, result_str);
			sprintf(value_name, "Q%d", i+1);
			sprintf(str_value, "%s var", result_str);
			cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
		}
	}

	//St,S1,S2,S3
	for(i = 0; i < number_of_P; i++)
	{
		//if(i == 0 || (((valid_measurements & (1<<(i-1))) == 0)))
		result_str[0] = '\0';
		convertModbusT5T6((uint8_t *)measurement_data, 8+2*i, 4, 1, result_str);
		//else
			//strcpy(result_str, " -- ");

		//sprintf(tmp_str, "\"S%d\":\"%sVA\",", i, result_str);
		sprintf(value_name, "S%d", i);
		sprintf(str_value, "%s VA", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));

		if(number_of_P == 1) //copy S0 to S1 because 1 phase counter has only P0
		{
			//sprintf(tmp_str, "\"S%d\":\"%sVA\",", i+1, result_str);
			sprintf(value_name, "S%d", i+1);
			sprintf(str_value, "%s VA", result_str);
			cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
		}
	}

	memset(measurement_data, 0, 64);
	ret = read_modbus_registers(USARTx, modbus_address, 30164, 16, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30164");
		statistics.errors++;
		goto end;
	}

	//PFt,PF1,PF2,PF3
	for(i = 0; i < number_of_P; i++)
	{
		//if(i == 0 || (((valid_measurements & (1<<(i-1))) == 0)))
		result_str[0] = '\0';
		convertModbusT7((uint8_t *)measurement_data, 0+2*i, result_str);
		//else
			//strcpy(result_str, " -- ");

		//sprintf(tmp_str, "\"PF%d\":\"%s\",", i, result_str);
		sprintf(value_name, "PF%d", i);
		sprintf(str_value, "%s", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));

		if(number_of_P == 1) //copy PF0 to PF1 because 1 phase counter has only P0
		{
			//sprintf(tmp_str, "\"PF%d\":\"%s\",", i+1, result_str);
			sprintf(value_name, "PF%d", i+1);
			cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(result_str));
		}
	}
#if 1
	//PAt,PA1,PA2,PA3
	for(i = 0; i < number_of_P; i++)
	{
		int16_t value;

		value = getModbusValue16((uint8_t *)measurement_data, 8+i);
		sprintf(result_str, "%d.%02d", value/100, abs(value%100));  //17.9.2019: printf library from tiny to small
		//sprintf(tmp_str, "\"PA%d\":\"%s\",", i, result_str);
		sprintf(value_name, "PA%d", i);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(result_str));

		if(number_of_P == 1) //copy PA0 to PA1 because 1 phase counter has only P0
		{
			//sprintf(tmp_str, "\"PA%d\":\"%s\",", i+1, result_str);
			sprintf(value_name, "PA%d", i+1);
			sprintf(str_value, "%s", result_str);
			cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
		}
	}
#endif

	//30181
	memset(measurement_data, 0, 64);
	ret = read_modbus_registers(USARTx, modbus_address, 30181, 11, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30181");
		statistics.errors++;
		goto end;
	}

	uint16_t val = getModbusValue16((uint8_t *)measurement_data, 0); //Tint at 30181
	sprintf(result_str, "%d.%02d", val/100, val%100);
	//sprintf(tmp_str, "\"Tint\":\"%s deg\",", result_str);
	sprintf(value_name, "Tint");
	sprintf(str_value, "%s", result_str);
	cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));

	//U1 THD,U2 THD,U3 THD 30182 - 30184
	for(i = 0; i < number_of_UI; i++)
	{
		val = getModbusValue16((uint8_t *)measurement_data, i+7);
		sprintf(result_str, "%d.%02d", val/100, val%100);
		//sprintf(tmp_str, "\"I%d%%\":\"%s%\",", i+1, result_str);
		sprintf(value_name, "THDI%d", i+1);  //sprintf(value_name, "I%d%%", i+1);
		sprintf(str_value, "%s", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(str_value));
	}

	//I1 THD,I2 THD,I3 THD 30188 - 30190
	for(i = 0; i < number_of_UI; i++)
	{
		val = getModbusValue16((uint8_t *)measurement_data, i+1);
		sprintf(result_str, "%d.%02d", val/100, val%100);
		//sprintf(tmp_str, "\"U%d%%\":\"%s%\",", i+1, result_str);
		sprintf(value_name, "THDUp%d", i+1); //sprintf(value_name, "U%d%%", i+1);
		//sprintf(str_value, "%s", result_str);
		cJSON_AddItemToObject(measurements, value_name, cJSON_CreateString(result_str));
	}

	//tariff
	ret = read_modbus_registers(USARTx, modbus_address, 30405, 1, (uint8_t *)measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30405");
		statistics.errors++;
		goto end;
	}


	uint16_t value = measurement_data[0]<<8 | measurement_data[1];
	//sprintf(tmp_str, "\"tariff\":\"%d\"", value);
	cJSON_AddItemToObject(measurements, "tariff", cJSON_CreateNumber(value));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print counters JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *getEnergyCountersJSON(int USARTx, int modbus_address, int number)
{
	char *string = NULL;
	cJSON *header = NULL;
	cJSON *counters = NULL;
	cJSON *param = NULL;
	//cJSON *counter_value = NULL;
	size_t i;
	char result_str[40];
	char measurement_data[128];
	int number_of_resetable_counters = 4;
	int number_of_non_resetable_counters = 4;
	int exponent_offset;
	int counter_offset;
	long value;
	int16_t exponent;
	char counter_name[15];
	char model[17];
	char counter_value[40];
	int ret;

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//create header object
	header = cJSON_CreateObject();
	if (header == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "header", header); //header name

	cJSON_AddItemToObject(header, "cmd", cJSON_CreateString("get_energy_counters"));
	cJSON_AddItemToObject(header, "number", cJSON_CreateNumber(number + 1));
	if(USARTx == RS485_UART_PORT)
	{
		cJSON_AddItemToObject(header, "RS485 index", cJSON_CreateNumber(status.modbus_device[number].index + 1));
		cJSON_AddItemToObject(header, "port", cJSON_CreateString("RS485"));
	}
	else if(USARTx == LEFT_IR_UART_PORT)
			cJSON_AddItemToObject(header, "port", cJSON_CreateString("Left IR"));

	cJSON_AddItemToObject(header, "modbus_addr", cJSON_CreateNumber(modbus_address));

	ret = getModelAndSerial(USARTx, modbus_address, model, (uint8_t *)measurement_data); //use measurement_data for serial number buffer
	if(ret < 0)
	{
		MY_LOGE(TAG, "getModelAndSerial failed, uart:%d, addr:%d",USARTx, modbus_address);
		cJSON_AddItemToObject(header, "getModelAndSerial", cJSON_CreateString("error"));
		statistics.errors++;
		goto end;
	}

	//MY_LOGI(TAG, "Model:%s serial:%s", model, measurement_data);
	cJSON_AddItemToObject(header, "model", cJSON_CreateString(model));
	cJSON_AddItemToObject(header, "serial_number", cJSON_CreateString(measurement_data));

	//description and location from instrument
	if(strncmp(model, "WM1", 3) == 0)
		ret = getDescriptionLocationSmall(USARTx, modbus_address, result_str, measurement_data); //use measurement_data for location buffer
	else
		ret = getDescriptionLocation(USARTx, modbus_address, result_str, measurement_data); //use measurement_data for location buffer

	if(ret < 0)
	{
		MY_LOGE(TAG, "getDescriptionLocation failed, uart:%d, addr:%d",USARTx, modbus_address);
		statistics.errors++;
		cJSON_AddItemToObject(header, "getDescriptionLocation", cJSON_CreateString("error"));
		//goto end;
	}

	cJSON_AddItemToObject(header, "location", cJSON_CreateString(measurement_data));
	cJSON_AddItemToObject(header, "description", cJSON_CreateString(result_str));

	//description from settings
	if(USARTx == RS485_UART_PORT)
		cJSON_AddItemToObject(header, "SG description", cJSON_CreateString(settings.rs485[status.modbus_device[number].index].description));
	else if(USARTx == LEFT_IR_UART_PORT)
		cJSON_AddItemToObject(header, "SG description", cJSON_CreateString(settings.left_ir_desc));

	cJSON_AddItemToObject(header, "local_time", cJSON_CreateString(status.local_time));

	//create counters object
	counters = cJSON_CreateObject();
	if (counters == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "counters", counters);

	if(strncmp(model, "WM", 2) == 0 || strncmp(model, "MC", 2) == 0 || strncmp(model, "iMC", 3) == 0)//WM's and MC's modbus table
	{
		//read configuration needed for result decoding
		ret = readWMCountersConfiguration(USARTx, modbus_address);
		if(ret)
		{
			MY_LOGE(TAG, "Failed to readCountersConfiguration");
			statistics.errors++;
			goto end;
		}

		if( (strncmp(model, "WM3M4", 5) == 0) || (strncmp(model, "7M.34.8", 7) == 0) )
		{
			number_of_non_resetable_counters = 2;
			number_of_resetable_counters = 0;
		}
		else if(strncmp(model, "WM3", 3) == 0)
		{
			number_of_non_resetable_counters = 4;
			number_of_resetable_counters = 4;
		}
		else if(strncmp(model, "WM1", 3) == 0 || strncmp(model, "MC", 2) == 0 || strncmp(model, "iMC", 3) == 0)
		{
			number_of_non_resetable_counters = 0;
			number_of_resetable_counters = 4; //non resetable at location of resetable
		}

		//read counter values
		if(strncmp(model, "WM1", 3) == 0)
			ret = read_modbus_registers(USARTx, modbus_address, 30400, 14, (uint8_t *)measurement_data);//WM1-6 reports error if we read 26 registers
		else
			ret = read_modbus_registers(USARTx, modbus_address, 30400, 26, (uint8_t *)measurement_data);

		if(ret)
		{
			MY_LOGE(TAG, "Failed to read registers at 30400");
			statistics.errors++;
			goto end;
		}

		for(i = 0; i < number_of_resetable_counters; i++)
		{
			exponent_offset = 1;
			counter_offset = 6;

			value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
			exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
			format_energy_counter(value, exponent, result_str);

			sprintf(counter_name, "counter%d", i+1);
			sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit

			cJSON_AddItemToObject(counters, counter_name, cJSON_CreateString(counter_value));
		}

		for(i = 0; i < number_of_non_resetable_counters; i++)
		{
			exponent_offset = 14;
			counter_offset = 18;

			value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
			exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
			format_energy_counter(value, exponent, result_str);

			sprintf(counter_name, "counter%dnr", i+1);
			sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit

			cJSON_AddItemToObject(counters, counter_name, cJSON_CreateString(counter_value));
		}
	}
	else if(strncmp(model, "IE", 2) == 0 || strncmp(model, "7M.", 3) == 0) //IE and finder modbus table, finder format:7M.24.8.230.0001
	{
		//read configuration needed for result decoding
		ret = readIECountersConfiguration(USARTx, modbus_address);
		if(ret)
		{
			MY_LOGE(TAG, "Failed to readCountersConfiguration");
			statistics.errors++;
			goto end;
		}

		if(strncmp(result_str, "IE1", 3) == 0 || strncmp(result_str, "7M.24", 5) == 0)//single phase meter
		{
			number_of_non_resetable_counters = 4;
			number_of_resetable_counters = 8;
		}
		else if(strncmp(result_str, "IE3", 3) == 0 || strncmp(result_str, "7M.38", 5) == 0)// 3-phase meter
		{
			number_of_non_resetable_counters = 4;
			number_of_resetable_counters = 16;
		}

		//read counter values
		ret = read_modbus_registers(USARTx, modbus_address, 30400, 62, (uint8_t *)measurement_data);
		if(ret)
		{
			MY_LOGE(TAG, "Failed to read registers at 30400");
			statistics.errors++;
			goto end;
		}

		for(i = 0; i < number_of_non_resetable_counters; i++)
		{
			exponent_offset = 1; //30401
			counter_offset = 6;	//30406

			value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
			exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
			format_energy_counter(value, exponent, result_str);

			sprintf(counter_name, "counter%dnr", i+1);
			sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit

			cJSON_AddItemToObject(counters, counter_name, cJSON_CreateString(counter_value));
		}

		for(i = 0; i < number_of_resetable_counters; i++)
		{
			exponent_offset = 46;
			counter_offset = 14;

			value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
			exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
			format_energy_counter(value, exponent, result_str);

			sprintf(counter_name, "counter%d", i+1);
			sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit

			cJSON_AddItemToObject(counters, counter_name, cJSON_CreateString(counter_value));
		}
	}
	else
	{
		MY_LOGE(TAG, "Unknown meter type: %s", model);
		statistics.errors++;
		goto end;
	}

	cJSON_AddItemToObject(header, "resetable_counters", cJSON_CreateNumber(number_of_resetable_counters));
	cJSON_AddItemToObject(header, "non_resetable_counters", cJSON_CreateNumber(number_of_non_resetable_counters));

	//create counter param object
	param = cJSON_CreateObject();
	if (param == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "settings", param);

	//phase
	for(i = 0; i < (number_of_non_resetable_counters + number_of_resetable_counters); i++)
	{
		if(i < number_of_non_resetable_counters)
			sprintf(counter_name, "phase%dnr", i+1);
		else
			sprintf(counter_name, "phase%d", i+1-number_of_non_resetable_counters); //resetable index starts with 1

		sprintf(counter_value, "%s", counter_phase[counterConfig[i].phase]);
		if(strncmp(result_str, "WM3", 3) == 0 || strncmp(result_str, "IE3", 3) == 0 || strncmp(result_str, "7M.38", 5)) //only for 3 phase meters
			cJSON_AddItemToObject(param, counter_name, cJSON_CreateString(counter_value));
		else
			cJSON_AddItemToObject(param, counter_name, cJSON_CreateString(""));
	}

	//tariff
	for(i = 0; i < (number_of_non_resetable_counters + number_of_resetable_counters); i++)
	{
		int j;

		if(i < number_of_non_resetable_counters)
			sprintf(counter_name, "tariff%dnr", i+1);
		else
			sprintf(counter_name, "tariff%d", i+1-number_of_non_resetable_counters); //resetable index starts with 1
		strcpy(counter_value, "");
		for(j = 0; j < 8; j++)
			if(counterConfig[i].tariff & (1 << j))
			{
				char tmp[3];
				sprintf(tmp, "%u ", j+1);
				strcat(counter_value, tmp);
			}

		cJSON_AddItemToObject(param, counter_name, cJSON_CreateString(counter_value));
	}

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print counters JSON");
		statistics.errors++;
	}
	else
		MY_LOGD(TAG, "%s", string);

end:
	if (json_root != NULL)
		cJSON_Delete(json_root);

	return string;
}

int getEnergyCounterValue(int USARTx, int modbus_address, energyCounterValuesStruct_t *counter_values)
{
	size_t i;
	//char result_str[40];
	char measurement_data[128];
	//int number_of_resetable_counters = 4;
	int number_of_non_resetable_counters = 4;
	int exponent_offset;
	int counter_offset;
	long value;
	int16_t exponent;
	//char counter_name[15];
	char model[17];
	//char counter_value[40];
	int ret;

	ret = getModelAndSerial(USARTx, modbus_address, model, (uint8_t *)measurement_data); //use measurement_data for serial number buffer
	if(ret < 0)
	{
		MY_LOGE(TAG, "getModelAndSerial failed, uart:%d, addr:%d",USARTx, modbus_address);
		statistics.errors++;
		return -1;
	}

	MY_LOGI(TAG, "Model:%s serial:%s", model, measurement_data);

	//offsets for most meters
	exponent_offset = 14;
	counter_offset = 18;

	if( (strncmp(model, "WM3M4", 5) == 0) || (strncmp(model, "7M.34.8", 7) == 0) )
	{
		number_of_non_resetable_counters = 2;
		//number_of_resetable_counters = 0;
	}
	else if(strncmp(model, "WM3", 3) == 0)
	{
		number_of_non_resetable_counters = 4;
		//number_of_resetable_counters = 4;
	}
	else if(strncmp(model, "WM1", 3) == 0 || strncmp(model, "MC", 2) == 0 || strncmp(model, "iMC", 3) == 0 || strncmp(model, "IE", 2) == 0 || strncmp(model, "7M.", 3) == 0)
	{
		number_of_non_resetable_counters = 4;
		//number_of_resetable_counters = 4; //non resetable at location of resetable
		exponent_offset = 1; //non resetable at location of resetable
		counter_offset = 6;
	}
	else
	{
		MY_LOGE(TAG, "Unknown meter type: %s", model);
		statistics.errors++;
		return -1;
	}

	//read counter values
	if(strncmp(model, "WM1", 3) == 0)
		ret = read_modbus_registers(USARTx, modbus_address, 30400, 14, (uint8_t *)measurement_data);//WM1-6 reports error if we read 26 registers
	else
		ret = read_modbus_registers(USARTx, modbus_address, 30400, 26, (uint8_t *)measurement_data);

	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30400");
		statistics.errors++;
		return -1;
	}

	for(i = 0; i < number_of_non_resetable_counters; i++)
	{
		value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
		exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
		//MY_LOGI(TAG, "Counter: %d value: %ld exponent: %d", i+1, value, exponent);
		//format_energy_counter(value, exponent, result_str);
		counter_values->value[i] = value*pow(10, exponent - 2); //0.1 kWh resolution
	}
	return 0;
}
//find which index has RS485 device with its modbus address
int getIndexFromAddress(int modbus_address)
{
	uint8_t i;
	for(i = 0; i < RS485_DEVICES_NUM; i++)
	{
		if(settings.rs485[i].address == modbus_address)
			return i;
	}

	MY_LOGE(TAG, "Failed to find RS485 device with %d modbus address", modbus_address);
	return -1; //not found
}

//find which number in status has RS485 device with its index
int getNumberFromIndex(int index)
{
	uint8_t i;
	for(i = 0; i < status.measurement_device_count; i++) //all modbus devices
	{
		if(status.modbus_device[i].index == index)
			return i;
	}

	MY_LOGE(TAG, "Failed to find RS485 device with %d index", index);
	return -1; //not found
}

int getRamLoggerData(int USARTx, int modbus_address)
{
	uint8_t measurement_data[200];  //we need only 96 samples for 24 hour at 15 minutes interval
	int i;
	int ret;
	int modbus_data;
	int no_samples;
	int block_size;
	int index;

	//default values for counter because it doesnt have that settings
	unsigned short Conn_mode  = 3; //40143 Connection mode
	unsigned long CT_Sec = 1; //40144 CT Secundary
	unsigned long CT_Pri = 1; //40145 CT Primary to mA
	unsigned long VT_Sec = 1; //40146 CT Secundary
	unsigned long VT_Pri = 1; //40147 CT Primary to mV
	unsigned short I_Ran  = 100; //40148 Current input range (%)
	unsigned short U_Ran  = 100; //40149 Voltage input range (%)

	unsigned long U_Cal;
	unsigned long I_Cal;

	if(USARTx == RS485_UART_PORT)
	{
		//pq meters have CT/VT
		index = getIndexFromAddress(modbus_address);
		if(index < 0)
			return -1;

		if(settings.rs485[index].type == RS485_PQ_METER)
		{
			//read setting 40144
			memset(measurement_data, 0, 14);
			ret = read_modbus_registers(USARTx, modbus_address, 40143, 7, measurement_data);
			if(ret)
			{
				MY_LOGE(TAG, "Failed to read registers at 40143");
				statistics.errors++;
				return -1;
			}

			Conn_mode = getModbusValue16(measurement_data, 0); //40143 connection mode
			CT_Sec = convertT4ToLong(getModbusValue16(measurement_data, 1)); //40144 CT Secondary
			CT_Pri = convertT4ToLong(getModbusValue16(measurement_data, 2))*100; //40145 CT Primary to mA
			VT_Sec = convertT4ToLong(getModbusValue16(measurement_data, 3)); //40146 CT Secondary
			VT_Pri = convertT4ToLong(getModbusValue16(measurement_data, 4))*100; //40147 CT Primary to mV
			I_Ran  = getModbusValue16(measurement_data, 5)/100; //40148 Current input range (%)
			U_Ran  = getModbusValue16(measurement_data, 6)/100; //40149 Voltage input range (%)
		}

		if(Conn_mode != 1) //1b connection mode (1 phase)
			Conn_mode = 3; //3 phase power
	}

	char model[16];
	ret = getModelAndSerial(USARTx, modbus_address, model, (uint8_t *)measurement_data); //use measurement_data for serial number buffer
	if(ret < 0)
	{
		MY_LOGE(TAG, "getModelAndSerial failed, uart:%d, addr:%d",USARTx, modbus_address);
		//cJSON_AddItemToObject(header, "getModelAndSerial", cJSON_CreateString("error"));
		statistics.errors++;
		//goto end;
	}
	else
	{
		if(strncmp(model, "WM1", 3) == 0 || strncmp(model, "IE1", 3) == 0 || strncmp(model, "7M.24", 5) == 0)
			Conn_mode = 1;
	}

	//read calibration data 30015
	memset(measurement_data, 0, 6);
	ret = read_modbus_registers(USARTx, modbus_address, 30015, 3, measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 30015");
		statistics.errors++;
		return -1;
	}

	if(((USARTx == RS485_UART_PORT) && (settings.rs485[index].type == RS485_PQ_METER))
			|| (strncmp(model, "WM3", 3) == 0)) //T4 format in PQ meters
	{
		U_Cal = convertT4ToLong(getModbusValue16(measurement_data, 0))/1000; //30015 Calibration Voltage
		I_Cal = convertT4ToLong(getModbusValue16(measurement_data, 2))/1000; //30017 Calibration Current
	}
	else //T16 format in energy counters
	{
		U_Cal = getModbusValue16(measurement_data, 0)/100; //30015 Calibration Voltage
		I_Cal = getModbusValue16(measurement_data, 2)/100; //30017 Calibration Current
	}

#if 0
	MY_LOGI(TAG, "Conn_mode:%d",Conn_mode);
	MY_LOGI(TAG, "CT_Sec:%ld",CT_Sec);
	MY_LOGI(TAG, "CT_Pri:%ld",CT_Pri);
	MY_LOGI(TAG, "VT_Sec:%ld",VT_Sec);
	MY_LOGI(TAG, "VT_Pri:%ld",VT_Pri);
	MY_LOGI(TAG, "I_Ran:%d",I_Ran);
	MY_LOGI(TAG, "U_Ran:%d",U_Ran);
	MY_LOGI(TAG, "U_Cal:%ld",U_Cal);
	MY_LOGI(TAG, "I_Cal:%ld",I_Cal);
#endif

	//read RAM logger parameters block at 36000
	memset(measurement_data, 0, 8);
	ret = read_modbus_registers(USARTx, modbus_address, 36000, 4, measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 36000");
		statistics.errors++;
		return -1;
	}

	modbus_data = getModbusValue16(measurement_data, 0); //36000 measurement parameter
	MY_LOGI(TAG, "parameter: %d", modbus_data);

	modbus_data = getModbusValue16(measurement_data, 1); //36001 time interval
	MY_LOGI(TAG, "interval: %d", modbus_data);

	no_samples = getModbusValue16(measurement_data, 2); //36002 number of results
	MY_LOGI(TAG, "RAM logger samples:%d",no_samples);

	if(no_samples < 1)
	{
		MY_LOGW(TAG, "No of samples < 1");
		//errorCounter++;
		return no_samples;
	}

	if(no_samples > 96)
		no_samples = 96; //thats all we need

	modbus_data = getModbusValue16(measurement_data, 3); //36003 last timestamp
	MY_LOGI(TAG, "timestamp: %d", modbus_data);

	//read RAM logger data block at 36004
	if(no_samples > 64)
		block_size = 64;
	else
		block_size = no_samples;

	memset(measurement_data, 0, 96*2);
	ret = read_modbus_registers(USARTx, modbus_address, 36004, block_size, measurement_data);
	if(ret)
	{
		MY_LOGE(TAG, "Failed to read registers at 36004");
		statistics.errors++;
		return -1;
	}

	if(no_samples > 64)
	{
		block_size = (no_samples - 64);//read the rest
		ret = read_modbus_registers(USARTx, modbus_address, 36068, block_size, measurement_data+128);
		if(ret)
		{
			MY_LOGE(TAG, "Failed to read registers at 36068");
			statistics.errors++;
			return -1;
		}
	}
//#define NEGATIVE_VALUES_TEST
#ifdef NEGATIVE_VALUES_TEST
	no_samples = 96;
#endif

	for (i = 0; i < no_samples; i++)
	{
		modbus_data = getModbusValue16(measurement_data, i);
		//MY_LOGI(TAG, "modbus_data[%d]: %d", i, modbus_data);

#ifdef NEGATIVE_VALUES_TEST
		if(i%2)
			modbus_data = -5000; //min value for test
		else
			modbus_data = 10000; //max value for test
#endif

		float value = (((CT_Pri/CT_Sec * U_Ran * U_Cal)/(float)100) * ((VT_Pri/VT_Sec * I_Ran * I_Cal)/(float)100) * Conn_mode * modbus_data) /(float)10000; //3phase
		//MY_LOGI(TAG, "value[%d]: %f", i, value);
		power_buf[i] = value;
	}
	return no_samples;
}

int readWMCountersConfiguration(int USARTx, int modbus_address)
{
  int ret;
  uint8_t configuration_data[70];
  uint16_t value;
  int i;
  int debug = 0;

  //read 25 registers
  memset(configuration_data, 0, 50);
  ret = read_modbus_registers(USARTx, modbus_address, 40421, 25, configuration_data);
  if(ret)
  {
	  MY_LOGE(TAG, "Failed to read registers at 40421");
	  statistics.errors++;
	  return -1;
  }

  for(i = 0; i < 4; i++)
  {
    int j = i;
    if(i == 3)//read new block
    {
      memset(configuration_data, 0, 10);
      ret = read_modbus_registers(USARTx, modbus_address, 40451, 4, configuration_data);
      if(ret)
      {
    	  MY_LOGE(TAG, "Failed to read registers at 40451");
    	  statistics.errors++;
    	  return -1;
      }
      j = 0;
    }
    //40421 +i*10
    value = getModbusValue16(configuration_data, j*10);
    counterConfig[i].phase = (value >> 2)&0x3;
    counterConfig[i].power = value&0x3;
    if(debug)
      MY_LOGI(TAG, "Counter%d phase:%d power:%d\n\r", i+1, counterConfig[i].phase, counterConfig[i].power);

    //40422 +i*10
    value = getModbusValue16(configuration_data, j*10 + 1);
    counterConfig[i].quadrant = value & 0xf;
    counterConfig[i].absolute = (value >> 4)&0x1;
    counterConfig[i].invert = (value >> 5)&0x1;
    if(debug)
      MY_LOGI(TAG, "Counter%d quadrant:%d absolute:%d invert:%d\n\r", i+1, counterConfig[i].quadrant, counterConfig[i].absolute, counterConfig[i].invert);

    //40424 +i*10
    value = getModbusValue16(configuration_data, j*10 + 3);
    counterConfig[i].tariff = value & 0x3;
    if(debug)
      MY_LOGI(TAG, "Counter%d tariff:%d\n\r", i+1, counterConfig[i].tariff);
  }

  return 0;
}

int readIECountersConfiguration(int USARTx, int modbus_address)
{
  int ret;
  uint8_t configuration_data[170];
  uint16_t value;
  int i;
  int debug = 0;

  //read 25 registers
  memset(configuration_data, 0, 170);
  ret = read_modbus_registers(USARTx, modbus_address, 40421, 80, configuration_data);
  if(ret)
  {
	  MY_LOGE(TAG, "Failed to read registers at 40421");
	  statistics.errors++;
	  return -1;
  }

  for(i = 0; i < 20; i++)
  {
    //int j = i;

    //Counter parameter 40421 +i*4
    value = getModbusValue16(configuration_data, i*4);
    counterConfig[i].phase = (value >> 2)&0x3;
    counterConfig[i].power = value&0x3;
    if(debug)
      MY_LOGI(TAG, "Counter%d phase:%d power:%d\n\r", i+1, counterConfig[i].phase, counterConfig[i].power);

    //Counter configuration 40422 +i*4
    value = getModbusValue16(configuration_data, i*4 + 1);
    counterConfig[i].quadrant = value & 0xf;
    counterConfig[i].absolute = (value >> 4)&0x1;
    counterConfig[i].invert = (value >> 5)&0x1;
    if(debug)
      MY_LOGI(TAG, "Counter%d quadrant:%d absolute:%d invert:%d\n\r", i+1, counterConfig[i].quadrant, counterConfig[i].absolute, counterConfig[i].invert);

    //Counter exponent 40423 +i*4
    value = getModbusValue16(configuration_data, i*4 + 2);
    counterConfig[i].exponent = value;
    if(debug)
    	MY_LOGI(TAG, "Counter%d exponent:%d\n\r", i+1, counterConfig[i].exponent);

    //Counter tariff selector 40424 +i*4
    value = getModbusValue16(configuration_data, i*4 + 3);
    counterConfig[i].tariff = value;
    if(debug)
      MY_LOGI(TAG, "Counter%d tariff:%02x\n\r", i+1, counterConfig[i].tariff);
  }

  if(debug)
	  MY_LOGI(TAG, "%s done\n\r", __FUNCTION__);

  return 0;
}

void format_energy_counter(long value, int8_t exponent, char *str_result)
{
	const char prefixes[] = {' ','k','M','G','T'};
	const long long DekL[5] = {1,1000,1000000,1000000000,1000000000000};
	int pe;
	int decimals;

	//MY_LOGI(TAG, "value: %ld exponent: %d", value, exponent);

	if(exponent < -3 || exponent > 11)//out of range?
	{
		sprintf(str_result, "N/A");
		return;
	}

	pe = (exponent+3)/3;

	if(exponent < 0)
		decimals = 0 - exponent; //exponent -3 = 3 decimals, exponent -2 = 2 decimal exponent -1 = 1 decimals
	else
		decimals = 3 - exponent%3; //exponent 1 = 2 decimals, exponent 2 = 1 decimal exponent 0 = 3 decimals

	//MY_LOGI(TAG, "value: %ld exponent: %d pe: %d decimals: %d", value, exponent, pe, decimals);
	sprintf(str_result, "%.*f %c", decimals, value*pow(10, exponent)/DekL[pe], prefixes[pe]);
	return;
}

void format_energy_counter_XML(long value, int8_t exponent, char *str_result)
{
	//MY_LOGI(TAG, "value: %ld exponent: %d", value, exponent);
	if(exponent)
		sprintf(str_result, "%lde%+02d", value, exponent);
	else //without exponent
		sprintf(str_result, "%ld", value);
	return;
}
