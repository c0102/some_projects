/*
 * push.c
 *
 *  Created on: Nov 25, 2019
 *      Author: iskra
 */

#include "main.h"

#ifdef TCP_CLIENT_ENABLED

#define XML_AVERAGE 2
#define XML_ACTUAL  3

extern const char *counter_units[];
//extern const char *counter_phase[];
//extern const char *counter_tariff[];

static const char * TAG = "push";

//char push_buf[4096];
static unsigned short average_measurements_counter;
static unsigned short last_average_measurements_counter = 0;
static int logId = 0;

void set_interval_measurements_parameters(int port, int address, int push_interval);

int createXmlTag(char *tag, char *unit, int val_mod, char *str_val, char *xml_buffer)
{
  char outTag[100];
  char * value_modifier[] = {"_min", "_max", "", "#"}; //min, max, average, actual

  char start_tag[] = {"<value ident=\""};
  char unit_tag[] = {"\" unit="};
  char end_tag[] = {"</value>"};
  char *p = outTag;
  outTag[0] = '\0';

  strcat(p, start_tag);
  strcat(p, tag); //name of tag
  strcat(p, value_modifier[val_mod]); //min, max ...
  //strcat(p, "#"); //todo min, max...
  strcat(p, unit_tag);
  strcat(p, "\""); //"
  strcat(p, unit); // V, A, Hz, ...
  strcat(p, "\">"); //"
  strcat(p, str_val); //measured value
  strcat(p, end_tag);
  strcat(p, "\n\r");

  //MY_LOGI(TAG, "XML%s", outTag);

  strcat(xml_buffer, outTag);

  return strlen(outTag);
}

//int getEnergyMeasurementsXML(USART_TypeDef *USARTx, int modbus_address)
int getEnergyMeasurementsXML(char *push_buf, int USARTx, int modbus_address, int push_interval)
{
  int ret;
  uint8_t measurement_data[64];
  char result_str[40];
  char tmp_str[200];
  char tag[50];
  int i;
  int number_of_P  = 4; //default for 3 phase
  int number_of_UI = 3;
  unsigned short val;
  char model_type[20];

  char *p = &push_buf[0]; //pointer for mystrcat

  //XML header
  sprintf(tmp_str, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\r");
  p = mystrcat(p, tmp_str); //strcpy(xml, tmp_str);

    //model_type and serial number
    ret = getModelAndSerial(USARTx, modbus_address, model_type, measurement_data); //use measurement_data for serial number buffer
    if(ret < 0)
    {
    	MY_LOGE(TAG, "getModelAndSerial failed");
    	return -1;
    }
    MY_LOGI(TAG, "Model:%s serial:%s", model_type, measurement_data);
    if(strncmp(model_type, "WM1", 3) == 0 || strncmp(model_type, "IE1", 3) == 0 || strncmp(result_str, "7M.24", 5) == 0)//single phase meter
    {
    	number_of_UI = 1;
    	number_of_P = 1;
    }

    sprintf(tmp_str, "<data logId=\"%d\" controlUnit=\"%s\" deviceType=\"%s\" part=\"%d\">\n\r",
    		logId++, measurement_data, model_type, 1);
    p = mystrcat(p, tmp_str);

    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
    	sprintf(result_str, "%s", "now"); //send now string, because we dont have valid time
    }
    else
    {
    	sprintf(result_str, "%d-%02d-%02dT%02d:%02d:%02dZ", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    if (push_interval > 900)
      sprintf(tmp_str, "<measurement dateTime=\"%s\" tInterval=\"PT%dM\">\n\r", result_str, push_interval / 60); //minutes
    else
      sprintf(tmp_str, "<measurement dateTime=\"%s\" tInterval=\"PT%03dS\">\n\r", result_str, push_interval);  //seconds

    p = mystrcat(p, tmp_str);

    memset(measurement_data, 0, 64);
    ret = read_modbus_registers(USARTx, modbus_address, 35500, 26, measurement_data);
    if(ret)
      return -1;

    average_measurements_counter = getModbusValue16(measurement_data, 2); //35502 Average measurements counter
    if(average_measurements_counter == 0)
    {
      MY_LOGW(TAG,"WARNING: No average measurements yet");
      //return -1;
    }
    else //read average measurements
    {
      printf("Last average interval duration   :%d\n\r", getModbusValue16(measurement_data, 0));//35500 last interval duration
      printf("Time since last measurements     :%d\n\r", getModbusValue16(measurement_data, 1));//35501 time since last measurements
      printf("Average measurements counter     :%d\n\r", average_measurements_counter);
      printf("Last Average measurements counter:%d\n\r", last_average_measurements_counter);
      printf("Timestamp (WM3)                  :%d\n\r", getModbusValue32(measurement_data, 3));//35503 timestamp
      //printf("Timestamp (SG) just seconds    :%d\n\r", timestamp%60);

      set_interval_measurements_parameters(USARTx, modbus_address, push_interval); //set average measurements interval for next push

      last_average_measurements_counter = average_measurements_counter;

      //frequency 35505 -35506
      convertModbusT5T6_XML(measurement_data, 5, 2, result_str);
      createXmlTag("f", "Hz", XML_AVERAGE, result_str, p);//sprintf(tmp_str, "\"frequency\":\"%sHz\",", result_str);

      //U1,U2,U3
      for(i = 0; i < number_of_UI; i++)
      {
        sprintf(tag, "U%d", i+1);
        convertModbusT5T6_XML(measurement_data, 7+2*i, 1, result_str);
        createXmlTag(tag, "V", XML_AVERAGE, result_str, p);
      }

      //U12,U23,U31
      for(i = 0; i < number_of_UI; i++)
      {
        sprintf(tag, "U%d%d", i+1, (i+2)>3?1:(i+2));
        convertModbusT5T6_XML(measurement_data, 18+2*i, 1, result_str);
        createXmlTag(tag, "V", XML_AVERAGE, result_str, p);
      }

      //35526
      memset(measurement_data, 0, 64);
      ret = read_modbus_registers(USARTx, modbus_address, 35526, 22, measurement_data);
      if(ret)
        return -1;
      //I1,I2,I3
      for(i = 0; i < number_of_UI; i++)
      {
        convertModbusT5T6_XML(measurement_data, 2*i, 2, result_str);

        sprintf(tag, "I%d", i+1);
        createXmlTag(tag, "A", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"I%d\":\"%sA\",", i+1, result_str);
      }

      //Pt,P1,P2,P3
      for(i = 0; i < number_of_P; i++)
      {
        convertModbusT5T6_XML(measurement_data, 14+2*i, 1, result_str);

        if(i == 0)
          sprintf(tag, "P");
        else
          sprintf(tag, "P%d", i);
        createXmlTag(tag, "W", XML_AVERAGE, result_str, p);//sprintf(tmp_str, "\"P%d\":\"%sW\",", i, result_str);

        if(number_of_P == 1) //copy P0 to P1 because 1 phase counter has only P0
        {
          sprintf(tag, "P%d", i+1);
          createXmlTag(tag, "W", XML_AVERAGE, result_str, p);//sprintf(tmp_str, "\"P%d\":\"%sW\",", i+1, result_str);
        }
      }

      //35548
      memset(measurement_data, 0, 64);
      ret = read_modbus_registers(USARTx, modbus_address, 35548, 16, measurement_data);
      if(ret)
        return -1;

      //Qt,Q1,Q2,Q3
      for(i = 0; i < number_of_P; i++)
      {
        convertModbusT5T6_XML(measurement_data, 2*i, 1, result_str);

        if(i == 0)
          sprintf(tag, "Q");
        else
          sprintf(tag, "Q%d", i);
        createXmlTag(tag, "var", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"Q%d\":\"%svar\",", i, result_str);

        if(number_of_P == 1) //copy Q0 to Q1 because 1 phase counter has only P0
        {
          sprintf(tag, "Q%d", i+1);
          createXmlTag(tag, "var", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"Q%d\":\"%svar\",", i+1, result_str);
        }
      }

      //St,S1,S2,S3
      for(i = 0; i < number_of_P; i++)
      {
        convertModbusT5T6_XML(measurement_data, 8+2*i, 1, result_str);

        sprintf(tag, "S%d", i);
        createXmlTag(tag, "VA", XML_AVERAGE, result_str, p);//sprintf(tmp_str, "\"S%d\":\"%sVA\",", i, result_str);

        if(number_of_P == 1) //copy S0 to S1 because 1 phase counter has only P0
        {
          sprintf(tag, "S%d", i+1);
          createXmlTag(tag, "VA", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"S%d\":\"%sVA\",", i+1, result_str);
        }
      }

      //35781
      memset(measurement_data, 0, 64);
      ret = read_modbus_registers(USARTx, modbus_address, 35581, 11, measurement_data);
      if(ret)
        return -1;

      val = getModbusValue16(measurement_data, 0); //Tint at 35781
      sprintf(result_str, "%d.%02d", val/100, val%100);
      createXmlTag("Tint", "deg", XML_AVERAGE, result_str, p);

      //U1 THD,U2 THD,U3 THD 35782 - 35784
      for(i = 0; i < number_of_UI; i++)
      {
        sprintf(tag, "U%d%%", i+1);
        val = getModbusValue16(measurement_data, i+1);
        sprintf(result_str, "%d.%02d", val/100, val%100);
        createXmlTag(tag, "%", XML_AVERAGE, result_str, p);
      }

      //I1 THD,I2 THD,I3 THD 35788 - 35790
      for(i = 0; i < number_of_UI; i++)
      {
        sprintf(tag, "I%d%%", i+1);
        val = getModbusValue16(measurement_data, i+7);
        sprintf(result_str, "%d.%02d", val/100, val%100);
        createXmlTag(tag, "%", XML_AVERAGE, result_str, p);
      }

#if 0
      //35564
      memset(measurement_data, 0, 64);
      ret = read_modbus_registers(USARTx, modbus_address, 35564, 16, measurement_data);
      if(ret)
        return -1;

      //PFt,PF1,PF2,PF3
      for(i = 0; i < number_of_P; i++)
      {
        convertModbusT7(measurement_data, 0+2*i, result_str);

        sprintf(tag, "PF%d", i);
        createXmlTag(tag, "", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"PF%d\":\"%s\",", i, result_str);

        if(number_of_P == 1) //copy PF0 to PF1 because 1 phase counter has only P0
        {
          sprintf(tag, "PF%d", i+1);
          createXmlTag(tag, "", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"PF%d\":\"%s\",", i+1, result_str);
        }
      }

      //PAt,PA1,PA2,PA3
      for(i = 0; i < number_of_P; i++)
      {
        int16_t value;

        value = getModbusValue16(measurement_data, 8+i);
        sprintf(result_str, "%d.%02d", value/100, abs(value%100));  //17.9.2019: printf library from tiny to small

        sprintf(tag, "PA%d", i);
        createXmlTag(tag, "", XML_AVERAGE, result_str, p); //sprintf(tmp_str, "\"PA%d\":\"%s\",", i, result_str);

        if(number_of_P == 1) //copy PA0 to PA1 because 1 phase counter has only P0
        {
          sprintf(tag, "PA%d", i);
          createXmlTag(tag, "", XML_AVERAGE, result_str, p);//sprintf(tmp_str, "\"PA%d\":\"%s\",", i+1, result_str);
        }
      }

      //tariff
      ret = read_modbus_registers(USARTx, modbus_address, 30405, 1, measurement_data);
      if(ret)
      {
        printf("Read tariff failed\n\r");
        //return -1;
      }
      else
      {
        uint16_t value = measurement_data[0]<<8 | measurement_data[1];
        sprintf(tmp_str, "\"tariff\":\"%d\"", value);
      }
#endif
    } //average measurements

    //counters
    int number_of_counters = 4; //minimum
    int counters_start_index = 0;

    if(strncmp(model_type, "WM", 2) == 0 || strncmp(model_type, "MC", 2) == 0)//WM's and MC's modbus table
    {
    	//read configuration needed for result decoding
    	ret = readWMCountersConfiguration(USARTx, modbus_address);
    	if(ret)
    	{
    		MY_LOGE(TAG, "Failed to readCountersConfiguration");
    		return -1;
    	}

    	if(strncmp(model_type, "WM3M4", 5) == 0)
    	{
    		number_of_counters = 2;
    		counters_start_index = 4;
    	}
    	else if(strncmp(model_type, "WM3", 3) == 0)
    	{
    		number_of_counters = 8;
    		counters_start_index = 0;
    	}
    	else if(strncmp(model_type, "WM1", 3) == 0 || strncmp(result_str, "MC", 2) == 0)
    	{
    		number_of_counters = 4;
    		counters_start_index = 0; //non resetable at location of resetable
    	}


    	memset(measurement_data, 0, 64);

    	//read counter values
    	if(strncmp(model_type, "WM1", 3) == 0)
    		ret = read_modbus_registers(USARTx, modbus_address, 30400, 14, measurement_data);//WM1-6 reports error if we read 26 registers
    	else
    	{
    		ret = read_modbus_registers(USARTx, modbus_address, 30400, 26, measurement_data);
    	}
    	if(ret)
    		return -1;

    	MY_LOGI(TAG, "Counters number:%d start:%d", number_of_counters, counters_start_index);
    	for(i = counters_start_index; i < counters_start_index + number_of_counters; i++)
    	{
    		int exponent_offset;
    		int counter_offset;
    		long value;
    		int16_t exponent;
    		char suffix[3];
    		if(i < 4)//resetable counters
    		{
    			exponent_offset = 1;
    			counter_offset = 6;
    			sprintf(suffix," ");
    		}
    		else //non resetable, (only on WM3)
    		{
    			exponent_offset = 10;
    			counter_offset = 10;
    			sprintf(suffix,"nr");
    		}

    		value = getModbusValue32(measurement_data, i*2 + counter_offset);
    		exponent = getModbusValue16(measurement_data, i + exponent_offset);
    		//formatT5T6(1, value, exponent, 0x80000000, 6, 2, result_str); //kW is default unit 26.4.2018: added sign
    		format_energy_counter_XML(value, exponent, result_str);

    		sprintf(tag, "E%d%s", ((i+1)>4)?(i+1-4):(i+1), suffix);
    		createXmlTag(tag, (char*)counter_units[counterConfig[(i>3)?i-4:i].power], XML_AVERAGE, result_str, p);
    		//sprintf(tmp_str, "\"counter%d%s\":\"%s%s\"", ((i+1)>4)?(i+1-4):(i+1), suffix, result_str, counter_units[counterConfig[(i>3)?i-4:i].power]);
    	}
    }//wm, mc
    else if(strncmp(model_type, "IE", 2) == 0 || strncmp(result_str, "7M.", 3) == 0) //IE modbus table
    {
    	int number_of_non_resetable_counters = 0;
    	int number_of_resetable_counters = 0;
    	int exponent_offset;
    	int counter_offset;
    	long value;
    	int16_t exponent;
    	char counter_value[40];

    	//read configuration needed for result decoding
    	ret = readIECountersConfiguration(USARTx, modbus_address);
    	if(ret)
    	{
    		MY_LOGE(TAG, "Failed to readCountersConfiguration");
    		goto push_end;
    	}

    	if(strncmp(model_type, "IE1", 3) == 0 || strncmp(result_str, "7M.24", 5) == 0)
    	{
    		number_of_non_resetable_counters = 4;
    		number_of_resetable_counters = 8;
    	}
    	else if(strncmp(model_type, "IE3", 3) == 0 || strncmp(result_str, "7M.38", 5) == 0)
    	{
    		number_of_non_resetable_counters = 4;
    		number_of_resetable_counters = 16;
    	}
    	else
    		goto push_end;

    	MY_LOGI(TAG, "Counters nr:%d resetable:%d", number_of_non_resetable_counters, number_of_resetable_counters);

    	//read counter values
    	ret = read_modbus_registers(USARTx, modbus_address, 30400, 62, (uint8_t *)measurement_data);
    	if(ret)
    		goto push_end;

    	for(i = 0; i < number_of_non_resetable_counters; i++)
    	{
    		exponent_offset = 1; //30401
    		counter_offset = 6;	//30406

    		value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
    		exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
    		format_energy_counter_XML(value, exponent, result_str);

    		sprintf(tag, "E%dnr", i+1);
    		//sprintf(tag, "E%d%s", ((i+1)>4)?(i+1-4):(i+1), suffix);
    		sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit
    		//MY_LOGI(TAG, "Counter:%d val:%s", i, counter_value);
    		createXmlTag(tag, (char*)counter_units[counterConfig[i].power], XML_AVERAGE, result_str, p);
    	}

    	for(i = 0; i < number_of_resetable_counters; i++)
    	{
    		exponent_offset = 46;
    		counter_offset = 14;

    		value = getModbusValue32((uint8_t *)measurement_data, i*2 + counter_offset);
    		exponent = getModbusValue16((uint8_t *)measurement_data, i + exponent_offset);
    		format_energy_counter_XML(value, exponent, result_str);

    		sprintf(tag, "E%d", i+1);
    		//sprintf(tag, "E%d%s", ((i+1)>4)?(i+1-4):(i+1), suffix);
    		sprintf(counter_value, "%s%s", result_str, counter_units[counterConfig[i].power]);	//value + unit
    		//MY_LOGI(TAG, "Counter:%d val:%s", i, counter_value);
    		createXmlTag(tag, (char*)counter_units[counterConfig[i+4].power], XML_AVERAGE, result_str, p);
    	}
    }//IE

    //end counters
push_end:
    sprintf(tmp_str, "</measurement>\n\r</data>\n\r");
    p = mystrcat(p, tmp_str);

    //end of measurements block

    MY_LOGI(TAG,"MEASUREMENTS XML Len:%d p:%d", strlen(push_buf), p-push_buf);
    //printf("%s", push_buf);

    return p-push_buf;
}

void set_interval_measurements_parameters(int port, int modbus_address, int push_interval)
{
	//todo: ali velja samo za števce...kako preprečiti različne nastavitve za drugi push/publish
  //interval duration
  if(push_interval > 3600 || push_interval < 1)
	  push_interval = 600; //default is 600 = 60 seconds

  modbus_write(port, modbus_address, 41990, push_interval*10, 1000 / portTICK_PERIOD_MS, NULL);

  //time to calculate (trigger)
  if(average_measurements_counter != last_average_measurements_counter) //set only if not retry
    modbus_write(port, modbus_address, 41991, push_interval*10 - 20, 1000 / portTICK_PERIOD_MS, NULL); //calculate average 2 seconds early to finish it before we read
}

#endif //TCP_CLIENT_ENABLED
