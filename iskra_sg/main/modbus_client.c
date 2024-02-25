/*
 * modbus_client.c
 *
 *  Created on: Nov 25, 2019
 *      Author: iskra
 */

#include "main.h"
#include <math.h>

extern u8_t tcp_obuf[RS485_RX_BUF_SIZE];

static const char *TAG = "modbus_client";

int read_modbus_reg_retry(int USARTx, int address, unsigned int startReg, unsigned int nrReg, uint8_t * data)
{
  char command[30];
  int messageDataSize = nrReg * 2 + 2;
  int functionCode;
  unsigned short crc;
  int size = 6;
  //int debug = 0;
  int offset = 0;
  //int port;
  char uart_msg[255];
  int bytes_received;

  //MY_LOGI(TAG,"%s: Usart:%d  Modbus address:0x%x start addr:0x%x nreg:0x%x",
      			//__FUNCTION__, USARTx, address, startReg, nrReg );
  command[0] = (char)address;	// modbus address
  if(address == 0) //broadcast: for finding RS485 devices; format is 0L????????
  {
    int i;
    command[1] = 'L';
    for(i = 0; i < 8; i++)
      command[i + 2] = data[i];

    size += 9; //9 additional bytes
    offset = 9;
  }

  if(startReg < 40000)
  {
    functionCode = 4;
    startReg = startReg - 30000;
  }
  else
  {
    functionCode = 3;
    startReg = startReg - 40000;
  }
  command[1 + offset] = (char)functionCode; // Function code for reading registers
  command[2 + offset] = (char)(startReg >> 8); // High word of register
  command[3 + offset] = (char)(startReg & 0xFF); // Low word of register
  command[4 + offset] = (char)(nrReg >> 8); // High word: count of registers to read
  command[5 + offset] = (char)(nrReg & 0xFF); // Low word: count of registers to read

  crc = CRC16( (unsigned char*) command, size);
  command[size] = (char)(crc >> 8);
  command[size + 1] = (char)(crc & 0xff);
  size += 2; //added crc

  if(address == 0)
    memset(uart_msg, 0,2);

  //----- SEND-RECEIVE TO/FROM SERIAL PORT -----
  memcpy(uart_msg, command, size);

  //MY_LOGW(TAG,"TCP_SEMAPHORE_TAKE");
  BaseType_t ret = xSemaphoreTake(tcp_semaphore, TCP_SEMAPHORE_TIMEOUT); //Wait for serial port to be ready
  if(ret != pdTRUE )
  {
	  MY_LOGW(TAG,"%s TCP_SEMAPHORE_TIMEOUT", __FUNCTION__);
	  //release semaphore even if we didn't took it
	  //xSemaphoreGive(tcp_semaphore);todo: possible bug
	  statistics.errors++;
	  return -1;
  }

  if(0) {}//just to enable conditional build

#ifdef ENABLE_LEFT_IR_PORT
  else if(USARTx == LEFT_IR_UART_PORT)
  {
	  if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
		  status.left_ir_busy = 1;//active
	  bytes_received = send_receive_modbus_serial(LEFT_IR_PORT_INDEX, uart_msg, size, MODBUS_RTU);
  }
#endif
#ifdef ENABLE_RS485_PORT
  else if(USARTx == RS485_UART_PORT)
	  bytes_received = send_receive_modbus_serial(RS485_PORT_INDEX, uart_msg, size, MODBUS_RTU);
#endif
  else
  {
	  MY_LOGW(TAG,"Wrong port %d, TCP_SEMAPHORE_GIVE", USARTx);
	  xSemaphoreGive(tcp_semaphore);
	  return -1;
  }

  //check response
  if(bytes_received == 0)
  {
	  MY_LOGW(TAG, "No response from port:%d", USARTx);
	  //MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
	  if(status.left_ir_busy == 1)//IR passthrough mode
		  status.left_ir_busy = 0;//not active
	  xSemaphoreGive(tcp_semaphore);
	  return -1;
  }

  if(address != 0)
  {
    if(tcp_obuf[0] != address || tcp_obuf[1] != functionCode || tcp_obuf[2] != nrReg*2)
    {
    	int i;
    	//int address, unsigned int startReg, unsigned int nrReg, uint8_t * data
    	MY_LOGE(TAG,"ERROR %s: Modbus address:0x%x-0x%x fc:%d-%d nreg:%d-%d",
    			__FUNCTION__, address, tcp_obuf[0], functionCode, tcp_obuf[1], nrReg*2,tcp_obuf[2] );
    	MY_LOGE(TAG,"ERROR %s: Sent Modbus address:0x%x start addr:0x%x nreg:%d data:",
    			__FUNCTION__, address, startReg, nrReg );
    	for(i = 0; i < size; i++)
    		printf("%x ", command[i]);
    	printf("\n\r");
    	MY_LOGE(TAG,"ERROR %s: Resp Size:%d Modbus address:0x%x func code:%d nreg:%d reg:%d fc:%d data:",
    			__FUNCTION__, bytes_received, tcp_obuf[0], tcp_obuf[1], tcp_obuf[2], startReg, functionCode );
    	for(i = 0; i < bytes_received; i++)
    		printf("%x ", tcp_obuf[i]);
    	printf("\n\r");
    	//statistics.errors++;
    	//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
    	if(status.left_ir_busy == 1)//IR passthrough mode
    		status.left_ir_busy = 0;//not active
    	xSemaphoreGive(tcp_semaphore);
    	return -1;
    }
  }
#if 0
  else if(address == 0 && data[1] == 'L' && bytes_received == 7) //address == 0
  {
    //MY_LOGI(TAG,"Detected address:%d size:%d", uart_msg[0], uart_msg_received[port]);
    if(noise_err_flag == 0)
    {
      uart_msg_received[port] = 0; //clear receive flag
      uart_msg_sent[port] = 0;
      return uart_msg[4];//return address
    }
    else
      return -1;
  }
#endif

  memcpy(data, tcp_obuf + 3, messageDataSize - 2);

  //MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
  if(status.left_ir_busy == 1)//IR passthrough mode
	  status.left_ir_busy = 0;//not active
  xSemaphoreGive(tcp_semaphore);

  return 0;
}

int read_modbus_registers(int USARTx, int address, unsigned int startReg, unsigned int nrReg, uint8_t * data)
{
  int ret =  -1;
  int retry = 3;

  if(address == 0)
    retry = 1;

  while(retry--)
  {
    ret = read_modbus_reg_retry(USARTx, address, startReg, nrReg, data);
    if(ret >= 0)
      break;
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  return ret;
}

int modbus_write(int USARTx, int modbus_address, unsigned int reg, unsigned short val, int timeout, char *serial_number)
{
  char modbus_command[20];
  int command_size = 8;
  int functionCode = 6;
  short crc;
  int bytes_received;
  int offs = 0;

  if(reg < 40000)
	  reg = reg - 30000;
  else
	  reg = reg - 40000;

  //MY_LOGI(TAG,"%s: Usart:%d  Modbus address:0x%x reg:0x%x val:0x%x",
        			//__FUNCTION__, USARTx, modbus_address, reg, val );
  modbus_command[0] = modbus_address;

  if(modbus_address == 0) //address by serial number
  {
	  int i;
	  modbus_command[1] = 'L';
	  for(i = 0; i < 8; i++)
		  modbus_command[i + 2] = serial_number[i];

	  command_size += 9;//9 additional bytes
	  offs = 9;
  }

  modbus_command[1 + offs] = functionCode; // Function code for write single register
  modbus_command[2 + offs] = (char)(reg >> 8); // High word of register
  modbus_command[3 + offs] = (char)(reg & 0xFF); // Low word of register
  modbus_command[4 + offs] = (char)(val >> 8); // register value High word
  modbus_command[5 + offs] = (char)(val & 0xFF); // register value Low word

  crc = CRC16((unsigned char*)modbus_command, command_size - 2); //crc is calculated on 6 bytes of command
  modbus_command[command_size - 2] = (char)(crc >> 8);
  modbus_command[command_size - 1] = (char)(crc & 0xff);

  //MY_LOGW(TAG,"TCP_SEMAPHORE_TAKE");
  BaseType_t ret = xSemaphoreTake(tcp_semaphore, TCP_SEMAPHORE_TIMEOUT); //Wait for serial port to be ready
  if(ret != pdTRUE )
  {
	  MY_LOGE(TAG,"TCP_SEMAPHORE_TIMEOUT");
	  //release semaphore even if we didnt took it
	  //xSemaphoreGive(tcp_semaphore);17.9.2020 bugfix?
	  statistics.errors++;
	  return -1;
  }

  if(0) {}//just to enable conditional build

#ifdef ENABLE_LEFT_IR_PORT
  else if(USARTx == LEFT_IR_UART_PORT)
  {
	  if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
		  status.left_ir_busy = 1;//active
	  bytes_received = send_receive_modbus_serial(USARTx, modbus_command, command_size, MODBUS_RTU);
  }
#endif
#ifdef ENABLE_RS485_PORT
  else if(USARTx == RS485_UART_PORT)
	  bytes_received = send_receive_modbus_serial(USARTx, modbus_command, command_size, MODBUS_RTU);
#endif
  else
  {
	  MY_LOGW(TAG,"Wrong port %d, TCP_SEMAPHORE_GIVE", USARTx);
	  //release semaphore
	  //MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
	  xSemaphoreGive(tcp_semaphore);
	  return -1;
  }

  if(status.left_ir_busy == 1)//IR passthrough mode
	  status.left_ir_busy = 0;//not active

  if(timeout == 0)//no response needed
  {
	  //MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
	  xSemaphoreGive(tcp_semaphore);
    return 0;
  }

  //release semaphore
  //MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
  xSemaphoreGive(tcp_semaphore);

  return bytes_received;
}

int getModelAndSerial(int USARTx, int modbus_address, char *model, uint8_t *serial)
{
  int i;
  int ret;
  uint8_t measurement_data[30];

  //memset(measurement_data, 0, 16);
  ret = read_modbus_registers(USARTx, modbus_address, 30001, 13, measurement_data);// 14.6.2021: 13 registers also gets device from terminal mode
  if(ret)
  {
	  ESP_LOGE(TAG, "Failed to read registers at 30001");
	  model[0] = 0;
	  serial[0] = 0;
	  statistics.errors++;
	  return -1;
  }

  //30001 Model number
  memset(model, 0, 16);
  for(i = 0;i < 16;i++)
  {
    if(measurement_data[i] == ' ') //end at 1st space
      break;

    model[i] = measurement_data[i];//offset is 1 short (2 bytes)
  }
  model[16] = 0;

  //30009 Serial number
  memset(serial, 0, 8);
  for(i = 0;i < 8;i++)
  {
	  if(measurement_data[i+16] == ' ') //end at 1st space
		  break;

	  serial[i] = measurement_data[i+16];//offset is 1 short (2 bytes)
  }
  serial[8] = 0;
  return 0;
}

int getDescriptionLocation(int USARTx, int modbus_address, char *description, char *location)
{
  int i;
  int ret;
  uint8_t measurement_data[80];

  //memset(measurement_data, 0, 16);
  ret = read_modbus_registers(USARTx, modbus_address, 40101, 40, measurement_data);
  if(ret)
  {
	  ESP_LOGE(TAG, "Failed to read %d registers at 40101", 40);
	  description[0] = 0;
	  location[0] = 0;
	  statistics.errors++;
	  return -1;
  }

  //40101 Description
  memset(description, 0, 40);
  for(i = 0;i < 40;i++)
    description[i] = measurement_data[i];

  description[39] = 0;

  //40121 Serial number
  memset(location, 0, 40);
  for(i = 0;i < 40;i++)
	  location[i] = measurement_data[i+40];

  location[39] = 0;
  return 0;
}

//same as above, but reading only 20 registers at once
int getDescriptionLocationSmall(int USARTx, int modbus_address, char *description, char *location)
{
  int i;
  int ret;
  uint8_t measurement_data[40];

  ret = read_modbus_registers(USARTx, modbus_address, 40101, 20, measurement_data);
  if(ret)
  {
	  ESP_LOGE(TAG, "Failed to read %d registers at 40101", 20);
	  description[0] = 0;
	  location[0] = 0;
	  statistics.errors++;
	  return -1;
  }

  //40101 Description
  memset(description, 0, 40);
  for(i = 0;i < 40;i++)
    description[i] = measurement_data[i];

  description[39] = 0;

  ret = read_modbus_registers(USARTx, modbus_address, 40121, 20, measurement_data);
  if(ret)
  {
	  ESP_LOGE(TAG, "Failed to read %d registers at 40121", 20);
	  location[0] = 0;
	  statistics.errors++;
	  return -1;
  }
  //40121 Location
  memset(location, 0, 40);
  for(i = 0;i < 40;i++)
	  location[i] = measurement_data[i];

  location[39] = 0;
  return 0;
}

uint8_t getModbusValue8(uint8_t *data, int offset)
{
  return(data[offset]);
}

int16_t getModbusValue16(uint8_t *data, int offset)
{
  return(data[offset*2]*0x100 + data[offset*2+1]);
}

int32_t getModbusValue32(uint8_t *data, int offset)
{
  return(data[offset*2]*0x1000000 + data[offset*2+1]*0x10000 + data[offset*2+2]*0x100 + data[offset*2+3]);
}

void convertModbusT5T6_XML(uint8_t *data, int offset, int required_precision, char *result_str)
{
  long value;
  int8_t exponent;
  int precision;

  //MY_LOGI(TAG,"%s off:%d %x%x%x%x", __FUNCTION__, offset, data[offset*2],data[offset*2+1], data[offset*2+2], data[offset*2+3]);
  exponent = data[offset*2];
  value = data[offset*2+1]*0x10000 + data[offset*2+2]*0x100 + data[offset*2+3];
  //result = value*pow(10, (double)exponent);
  //MY_LOGI(TAG,"%f", value*pow(10, exponent));

  if(required_precision != 0)
    precision = required_precision;
  else
  {
//  precision = 6 - (int)log10(value);
  precision = (abs)((int)exponent);
  if(precision < 0)
    precision = 0;

  if(precision > 4)
    precision = 4;
  }

  sprintf(result_str, "%.*f", precision, value*pow(10, exponent));
  //formatT5T6(1, value, exponent, 0x800000, digits, unit, result_str);
}

const long DekL[10] = {1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};

void formatT5T6(int sign_, long value, int8_t exponent, int max_val, int digits, int unit, char *str_result)
{
  /*
  unit: 0, leave it
        1, force unit withot prefix (A, V, ...)
        2, for k prefix (kW...)
        3, force M prefix (MW)
  */
  const char prefixes[] = {'n','u','m',' ','k','M','G','T'};
  int pe = 3; //prefix ' '
  char str_temp[20];
  int limit = DekL[digits];
  //char sign_char = ' ';

  //MY_LOGI(TAG,"%s sign:%d value:%d exp:%d", __FUNCTION__, sign_, value, exponent);
  //if (sign_ && value >= 0x800000)  //negativna vrednost
  if (sign_ && max_val == 0x800000 && value >= max_val)  //24 bit negative value
  {
    value = 0x1000000-value; //24 bit value
    value = abs(value);
    sprintf(str_result, "-");
  }
  else if(sign_ && max_val == 0x80000000 && value < 0 ) //32 bit negative value
  {
    value = abs(value);
    sprintf(str_result, "-");
  }
  //else
    //sprintf(str_result, "");

  //to positive exponent
  while (exponent < 0)
  {
    pe -= 1; //decrement prefix
    exponent+=3;
  }

  double result = value * DekL[exponent];
  int celo = (int)result;
  int decimal = (int)((result - (double)celo)*1000);

  //limit digits and force bigger units
  while (celo>limit-1 || (unit > 0 && (pe < (2 + unit))))
  {
    int cc  = celo/1000;
    decimal = (celo%1000);
    celo = cc;
    pe++;
  }

  //limit decimals: 0-99=3 decimals, 100-9999= 2 decimals 10000-99999=1 decimal
  int i;
  int decimal_numbers = 3;
  const long DecL[10] = {100,10000,100000};
  for(i = 0; i < 3; i++)
  {
    if(celo >=  DecL[i])
    {
      decimal_numbers--;
      decimal /= 10;
    }
    if(decimal_numbers == 0)
      break;
  }

  //cut zeroes at end of decimals
  while(decimal_numbers > 1)
  {
    if(decimal % 10 == 0) //if ends with 10 cut down 1 decimal number
    {
      decimal_numbers--;
      decimal /= 10;
    }
    else
       break;
  }

  //prepare final string with leading 0's for decimals
  if(decimal_numbers == 3 && decimal < 10)
      sprintf(str_temp, "%d.00%d ", celo, decimal);
  else if((decimal_numbers == 3 && decimal < 100) || (decimal_numbers == 2 && decimal < 10))
      sprintf(str_temp, "%d.0%d ", celo, decimal);
  else if(decimal_numbers != 0)
      sprintf(str_temp, "%d.%d ", celo, decimal);
  else
    sprintf(str_temp, "%d ", celo);

  if(pe != 3)//add prefix
  {
    char tmp[2];
    sprintf(tmp, "%c", prefixes[pe]);
    strcat(str_temp, tmp);
  }

  strcat(str_result, str_temp);

  return;
}

void convertModbusT5T6(uint8_t *data, int offset, int digits, int unit, char *result_str)
{
  long value;
  int16_t exponent;

  //MY_LOGI(TAG,"%s off:%d %x%x%x%x", __FUNCTION__, offset, data[offset*2],data[offset*2+1], data[offset*2+2], data[offset*2+3]);
  exponent = data[offset*2];
  value = data[offset*2+1]*0x10000 + data[offset*2+2]*0x100 + data[offset*2+3];
  formatT5T6(1, value, exponent, 0x800000, digits, unit, result_str);
}

void convertModbusT7(uint8_t *data, int offset, char *result_str)
{
  char import_export[7];
  char inductive_capacitive[5];
  unsigned short val;

  //MY_LOGI(TAG,"%s off:%d %x %x %x %x", __FUNCTION__, offset, data[offset*2],data[offset*2+1], data[offset*2+2], data[offset*2+3]);
  if(data[offset*2] == 0xff)
    strcpy(import_export, "Export");
  else
    strcpy(import_export, "Import");

  if(data[offset*2+1] == 0xff)
    strcpy(inductive_capacitive, "Cap");
  else
    strcpy(inductive_capacitive, "Ind");

  val = data[offset*2+2] << 8 | (data[offset*2+3] & 0xff);

  sprintf(result_str, "%d.%d %s",val/10000, abs(val%10000), inductive_capacitive);
  //sprintf(str_result, "0.%04d %s %s",val, import_export, inductive_capacitive);
}

unsigned long convertT4ToLong(unsigned short modbus_val)
{
  int8_t exponent = (modbus_val & 0xc000)>>14;
  uint32_t value = modbus_val & 0x3fff;
  return value * DekL[exponent];
}
