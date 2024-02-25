/*
 * utils.c
 *
 *  Created on: Nov 14, 2019
 *      Author: iskra
 */

#include "main.h"

static const char *TAG = "utils";

int read_file(char *filename, char *out, int size)
{
	FILE *fp;
	int c;

	struct stat entry_stat;
	if (stat(filename, &entry_stat) == -1) {
		MY_LOGE(TAG, "Failed to stat %s", filename);
		return -1;
	}

	if(entry_stat.st_size > size)
	{
		MY_LOGE(TAG, "%s size: %ld > buffer size:%d", filename, entry_stat.st_size, size);
		return -1;
	}

	fp = fopen(filename,"r");
	while(1) {
      c = fgetc(fp);
      if( feof(fp) ) {
         break;
      }
      //printf("%c", c);
      *out++ = c;
   }
   fclose(fp);
   return ESP_OK;
}

//------------------------------------------------------
int writeFile(char *fname, char *buf)
{
	FILE *fd = fopen(fname, "wb");
    if (fd == NULL) {
    	MY_LOGE("[write]", "fopen failed");
    	return -1;
    }
    int len = strlen(buf);
	int res = fwrite(buf, 1, len, fd);
	if (res != len) {
		MY_LOGE("[write]", "fwrite failed: %d <> %d ", res, len);
        res = fclose(fd);
        if (res) {
            ESP_LOGE("[write]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
	res = fclose(fd);
	if (res) {
		MY_LOGE("[write]", "fclose failed: %d", res);
    	return -4;
	}
    return 0;
}

/******************************************************************************/
/* Modbus functions
*******************************************************************************/
const unsigned char auchCRCHi[] = {
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80,
  0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
  0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80,
  0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
  0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
  0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80,
  0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
  0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
  0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
  0xC1, 0x81, 0x40 } ;

/* Table of CRC values for low-order byte */

const unsigned char auchCRCLo[] = {
  0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07,
  0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF,
  0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8,
  0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F,
  0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16,
  0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30,
  0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5,
  0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE,
  0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9,
  0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
  0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22,
  0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
  0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64,
  0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A,
  0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
  0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C,
  0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3,
  0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53,
  0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C,
  0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
  0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A,
  0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 0x44, 0x84,
  0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41,
  0x81, 0x80, 0x40 } ;

unsigned short CRC16(unsigned char *puchMsg, int usDataLen)
{
  //  extern unsigned char auchCRCHi[], auchCRCLo[];

  unsigned char uchCRCHi = 0xFF ; /* high CRC byte initialized */
  unsigned char uchCRCLo = 0xFF ; /* low CRC byte initialized  */
  unsigned uIndex ;               /* will index into CRC lookup*/
  /* table  */
  //printf("in crc\n");
  while (usDataLen--)             /* pass through message buffer   */
  {
    uIndex = uchCRCHi ^ *puchMsg++ ;       /* calculate the CRC  */
    uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
    uchCRCLo = auchCRCLo[uIndex] ;
    //printf("while = %d \n",usDataLen);
  }
  //printf("out crc\n");
  return ((uchCRCHi << 8) | uchCRCLo) ;
}

void CRC_update()
{
	status.settings_crc = CRC16((unsigned char *)&settings, sizeof(settings));
	save_u16_key_nvs("settings_crc", status.settings_crc);
}

char* mystrcat( char* dest, char* src )
{
     while (*dest) dest++;
     while ((*dest++ = *src++));
     return --dest;
}

unsigned short SwapW(unsigned short i)
{
  return (i<<8)|(i>>8);
}

int convert_to_rtu(char *raw_msg, int raw_size, int *tcp_modbus)
{
  int command_size;
  unsigned short crc;

  *tcp_modbus = check_tcp_header(raw_msg, raw_size);

  if(*tcp_modbus == 1)
  {
    //convert Modbus TCP to Modbus RTU
    /*
    The equivalent request to this Modbus RTU example
                   11 03 006B 0003 7687
    in Modbus TCP is:
    0001 0000 0006 11 03 006B 0003
*/
    command_size = *(((char *)raw_msg) + 4)*256 + *(((char *)raw_msg) + 5);
    /*if(command_size > TCP_IBUF_SIZE - 2)
    {
    printf("***ERROR TCP IBUF len: %d\n\r", command_size);
    errorCounter++;
    return;
  }*/

    crc = CRC16( (unsigned char*) raw_msg + 6, command_size);
    raw_msg[raw_size] = (char)(crc >> 8);
    raw_msg[raw_size + 1] = (char)(crc & 0xff);
    command_size += 2; //added crc
  }
  else //rtu
  {
    command_size = raw_size;
    /*if(command_size > TCP_IBUF_SIZE)
    {
    printf("***ERROR TCP IBUF len: %d\n\r", command_size);
    errorCounter++;
    return;
  }*/

  }
  return command_size;
}

char *get_spiffs_files_json(char *file_ext)
{
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
	char *string = NULL;
	char fullpath[FILE_PATH_MAX];
	char entrysize[16];
	const char *entrytype;

	DIR *dir = NULL;
	struct dirent *entry;
	struct stat entry_stat;
	cJSON *files = NULL;

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//find .pem files on SPIFFS
	strcpy(fullpath, "/spiffs/");   //zurb
	dir = opendir("/spiffs");
	const size_t entrypath_offset = strlen(fullpath);

	if (!dir) {
		MY_LOGE(TAG, "Failed to stat dir : %s", fullpath);
		goto end;
	}

	files = cJSON_CreateArray();
	if (files == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "files", files);

	while ((entry = readdir(dir)) != NULL) {
		entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

		strncpy(fullpath + entrypath_offset, entry->d_name, sizeof(fullpath) - entrypath_offset);
		if (stat(fullpath, &entry_stat) == -1) {
			MY_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
			continue;
		}
		sprintf(entrysize, "%ld", entry_stat.st_size);
		//MY_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
		if(*file_ext != '*') //* is without filtering
		{
			//MY_LOGI(TAG, "file:%s size:%d ext:%s size:%d", entry->d_name, strlen(entry->d_name), file_ext, strlen(file_ext));
			if(!IS_FILE_EXT(entry->d_name, file_ext))//return only files with selected mask
				continue;
		}

		MY_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
		cJSON_AddItemToArray(files, cJSON_CreateString(entry->d_name));
	}

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

const char* console_apps1[] = {"ADC","ATECC608A","bicom","control_task","DIG_INPUT","energy meter","ethernet_init","graph",
		"http_server","LED","LEFT_IR","main","modbus_client","modbus_slave","MQTT","NTP Client"}; //16 tags is max
const char* console_apps2[] = {"nvs","ota","push","RS485_APP","TCP_client","TCP Server","UDP","utils",
		"wifi_init","eeprom","","","","","",""};

void set_log_level1(int tag_id, int level)
{
	//1st modbus log register
	for(int i = 0; i < 16; i++)
	{
		if(tag_id & (1<<i))
			esp_log_level_set(console_apps1[i], level); //raised log level
		else
			esp_log_level_set(console_apps1[i], 3); //default log level
	}
}
void set_log_level2(int tag_id, int level)
{
	//2nd modbus log register
	for(int i = 0; i < 16; i++)
	{
		if(tag_id & (1<<i))
			esp_log_level_set(console_apps2[i], level); //raised log level
		else
			esp_log_level_set(console_apps2[i], 3); //default log level
	}
}

int checkTagLoglevel(const char *tag)
{

	for(int i = 0; i < 16; i++)
	{
		if((strcmp(console_apps1[i], tag) == 0) && ((settings.log_disable1 & (1<<i)) > 0))
			return 1;
	}

	for(int i = 0; i < 16; i++)
	{
		if((strcmp(console_apps2[i], tag) == 0) && ((settings.log_disable2 & (1<<i)) > 0))
			return 1;
	}

	return 0;
}

void send_info_to_left_IR(int time)
{
	//if(strncmp(status.detected_devices[0].model, "WM16", 4) || strncmp(status.detected_devices[0].model, "WM3", 3))//todo : ali naj posiljamo tudi ko ga ni?
	if(1)
	{
		int wifi_status_reg = 42750;
		//if(WM_SW_Version < 81)
			//wifi_status_reg = 41000;

		modbus_write(LEFT_IR_UART_PORT, 33, wifi_status_reg, time, 1000, NULL); //Wifi LCD menu time enabled
		modbus_write(LEFT_IR_UART_PORT, 33, wifi_status_reg + 1, status.connection, 1000, NULL); //status

		int n1, n2, n3, n4;
		sscanf(status.ip_addr, "%d.%d.%d.%d", &n1, &n2, &n3, &n4);
		modbus_write(LEFT_IR_UART_PORT, 33, wifi_status_reg + 2, (n1<<8 | n2), 1000, NULL); //ip address high
		modbus_write(LEFT_IR_UART_PORT, 33, wifi_status_reg + 3, (n3<<8 | n4), 1000, NULL); //ip address low
	}
}

int detect_terminal_mode(u8_t *data)
{
  if(data[1] != '$')
    return 0;

  if(data[2] != '*')
    return 0;

  if(data[4] != '!')
    return 0;

  if(data[5] != 't')
    return 0;

  printf("Terminal mode DETECTED\n\r");
  //terminal_mode_activity = 0;
  return 1;
}

int detect_terminal_mode_end(u8_t *data)
{
  if(data[1] != 'M')
    return 1;

  if(data[2] != 'O')
    return 1;

  if(data[3] != 'D')
    return 1;

  if(data[4] != 'B')
    return 1;

  if(data[5] != 'U')
    return 1;

  if(data[6] != 'S')
    return 1;

  printf("Terminal mode END DETECTED\n\r");
  return 0; //terminal mode in now 0
}

int countActiveRS485Devices()
{
	int i;
	int count = 0;

	for(i = 0; i < RS485_DEVICES_NUM; i++)
		if(settings.rs485[i].type != 0)//rs485 is active?
			count++;

	return count;//number of active rs485 device
}
void delay_us(int delay)
{
	int64_t start = esp_timer_get_time();

	while(esp_timer_get_time() < start + delay)
	{}
}

void printUptime(int uptime, char *time_str)
{
  const int minute = 60;
  const int hour = minute * 60;
  const int day = hour * 24;
  snprintf (time_str, 20, "%dd %02d:%02d:%02d", uptime / day, (uptime % day) / hour, (uptime % hour) / minute, uptime % minute);
}

void print_local_time(int timestamp, char *res)
{
	time_t t = timestamp;
	struct tm lt;
	localtime_r(&t, &lt);
	strftime(res, 32,"%d.%m.%Y %H:%M:%S", &lt);
	//MY_LOGI(TAG, "%s", res);
}

int getDayFromTime(int timestamp)
{
	time_t t;

	if(timestamp > 0)
		t = timestamp;
	else
		time(&t);

	struct tm * timeinfo = localtime(&t);
	return(timeinfo->tm_mday);
}

int getMonthFromTime(int timestamp)
{
	time_t t;

	if(timestamp > 0)
		t = timestamp;
	else
		time(&t);

	struct tm * timeinfo = localtime(&t);
	return(timeinfo->tm_mon);
}

int getYearFromTime(int timestamp)
{
	time_t t;

	if(timestamp > 0)
		t = timestamp;
	else
		time(&t);

	struct tm * timeinfo = localtime(&t);
	return(timeinfo->tm_year);
}
