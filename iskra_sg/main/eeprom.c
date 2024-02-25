
#include "main.h"

#ifdef EEPROM_STORAGE
//#include "eeprom.h"

#define I2C_MASTER_NUM  I2C_NUM_0
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_FREQ_HZ  100000

#define EEPROM_WRITE_ADDR   0x00
#define EEPROM_READ_ADDR    0x01

#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define ACK		(1)
#define NO_ACK	(0)

static const char *TAG = "eeprom";
static SemaphoreHandle_t i2c_Semaphore;

extern uint8_t eeprom_size;

#if 0
static int do_i2cdetect_cmd()
{
	uint8_t address;
	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
	for (int i = 0; i < 128; i += 16) {
		printf("%02x: ", i);
		for (int j = 0; j < 16; j++) {
			fflush(stdout);
			address = i + j;
			i2c_cmd_handle_t cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, NACK_VAL);
			i2c_master_stop(cmd);
			esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_RATE_MS);
			i2c_cmd_link_delete(cmd);
			if (ret == ESP_OK) {
				printf("%02x ", address);
			} else if (ret == ESP_ERR_TIMEOUT) {
				printf("UU ");
			} else {
				printf("-- ");
			}
			vTaskDelay(20/portTICK_PERIOD_MS);
		}
		printf("\r\n");
	}

	i2c_unlock_mutex();
    //i2c_driver_delete(i2c_port);
    return 0;
}
#endif

void i2c_init()
{
	i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
	i2c_master_driver_initialize();
	vTaskDelay(100/portTICK_PERIOD_MS);
}

#define I2C_MASTER_SCL_IO_16   5
#define I2C_MASTER_SDA_IO_16   0

//HW bug, switched lines
#define I2C_MASTER_SCL_IO_64   0
#define I2C_MASTER_SDA_IO_64   5

esp_err_t i2c_master_driver_initialize(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_64,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO_64,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    if(eeprom_size == 16)//24lc16
    {
    	conf.scl_io_num = I2C_MASTER_SCL_IO_16;
    	conf.sda_io_num = I2C_MASTER_SDA_IO_16;
    }
    //return i2c_param_config(I2C_NUM_0, &conf);
    i2c_param_config(I2C_MASTER_NUM, &conf);

    //Create the semaphore for i2c port
    i2c_Semaphore = xSemaphoreCreateMutex();

    //Assume i2c is ready
    i2c_unlock_mutex();

    //do_i2cdetect_cmd();

    return ESP_OK;
}

//24LC64
esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte) {
    esp_err_t ret;

    if(i2c_lock_mutex())
    {
    	MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
    	statistics.eeprom_errors++;
    	return ESP_FAIL;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_WRITE_ADDR, ACK_VAL);
    i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL);
    i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL);
    i2c_master_write_byte(cmd, byte, ACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(20/portTICK_PERIOD_MS);

    i2c_unlock_mutex();

    return ret;
}


uint8_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data) {

	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_WRITE_ADDR, ACK_VAL);
    i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL);
    i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_READ_ADDR, ACK_VAL);

    //uint8_t data;
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500/portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    	MY_LOGE(TAG, "%s", __FUNCTION__);
    i2c_cmd_link_delete(cmd);

    i2c_unlock_mutex();

    return ret;
}


#if 0
uint8_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress)
{
	uint8_t high_addr = (eeaddress >> 8) & 0xff;
	uint8_t low_addr = eeaddress & 0xff;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, deviceaddress << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, high_addr, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, low_addr, ACK_CHECK_EN);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, deviceaddress << 1 | I2C_MASTER_READ, ACK_CHECK_EN);
	uint8_t data;
	i2c_master_read_byte(cmd, &data, NACK_VAL);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return data;
}
#endif

#if 1 //#ifdef EEPROM_24LC16
esp_err_t eeprom_read(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data, size_t size) {
	esp_err_t ret = ESP_OK;
	//uint8_t data;
	int retry = 3;

	//MY_LOGE(TAG, "address: 0x%x", eeaddress);

	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	while(retry--)
	{
		vTaskDelay(20/portTICK_PERIOD_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		if(eeprom_size == 16)//24lc16
		{
			i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK_VAL); //control byte
			i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL); //address
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1)| EEPROM_READ_ADDR, ACK_VAL); //control byte
		}
		else //24lc64
		{
			i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_WRITE_ADDR, ACK_VAL);  //control byte
			i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL);
			i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL); //address
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_READ_ADDR, ACK_VAL);  //control byte
		}

		if (size > 1) {
			i2c_master_read(cmd, data, size-1, ACK_VAL);
		}

		//for(int i = 0; i < size -1; i++)
			//i2c_master_read_byte(cmd, data+i, ACK_VAL);

		i2c_master_read_byte(cmd, data+size-1, NACK_VAL);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			//MY_LOGE(TAG, "%s OK", __FUNCTION__);
			status.error_flags &= ~ERR_EEPROM;//clear error flag
			break;
		} else if (ret == ESP_ERR_TIMEOUT) {
			MY_LOGW(TAG, "%s timeout", __FUNCTION__);
			statistics.eeprom_errors++;
			status.error_flags |= ERR_EEPROM;
		} else {
			MY_LOGW(TAG, "%s ERR", __FUNCTION__);
			statistics.eeprom_errors++;
			status.error_flags |= ERR_EEPROM;
		}
	}

	//for(int i = 0; i < size; i++)
		//printf("eeprom_read: %d\n\r", data[i] );

	i2c_unlock_mutex();

    return ret;
}
#else //24LC64
esp_err_t eeprom_read(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size) {

	int ret;
	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	int retry = 1;
	while(retry--)
	{
		vTaskDelay(20/portTICK_PERIOD_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_WRITE_ADDR, ACK_VAL);  //control byte
		i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL);
		i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL); //address
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_READ_ADDR, ACK_VAL);  //control byte

		if (size > 1) {
			i2c_master_read(cmd, data, size-1, ACK_VAL);
		}

		//for(int i = 0; i < size -1; i++)
		//i2c_master_read_byte(cmd, data+i, ACK_VAL);

		i2c_master_read_byte(cmd, data+size-1, NACK_VAL);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			//MY_LOGE(TAG, "%s OK", __FUNCTION__);
			break;
		} else if (ret == ESP_ERR_TIMEOUT) {
			MY_LOGW(TAG, "%s timeout", __FUNCTION__);
			statistics.eeprom_errors++;
		} else {
			MY_LOGW(TAG, "%s ERR: %d", __FUNCTION__, ret);
			statistics.eeprom_errors++;
		}
	}

    i2c_unlock_mutex();
    return ret;
}
#endif //24LC64

#if 1 //#ifdef EEPROM_24LC16
esp_err_t eeprom_page_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size)
{
	esp_err_t ret = ESP_OK;

	//for(int i = 0; i < size; i++)
		//printf("eeprom_page_write: %d\n\r", data[i] );

	//MY_LOGI(TAG, "eeprom_page_write addr:0x%x size:%d block:%02x data:%02x %02x %02x %02x", eeaddress, size, (((eeaddress>>8)&0x7)<<1), *(data+0), *(data+1), *(data+2), *(data+3));
	if(size > EEPROM_PAGE_SIZE || eeaddress%EEPROM_PAGE_SIZE )
	{
		MY_LOGE(TAG, "%s ALIGNMENT EE address:%x or size:%x", __FUNCTION__, eeaddress, size);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	int retry = 3;

	while(retry--)
	{
		vTaskDelay(20/portTICK_PERIOD_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		if(eeprom_size == 16)//24lc16
		{
			i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK_VAL); //control byte
		}
		else //24lc64
		{
			i2c_master_write_byte(cmd, (deviceaddress << 1) | EEPROM_WRITE_ADDR, ACK_VAL);//control byte
			i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL); //address high byte
		}

		i2c_master_write_byte(cmd, eeaddress&0xff, ACK_VAL); //address
		for(int i = 0; i < size; i++)
			i2c_master_write_byte(cmd, *(data+i), ACK_VAL); //data write
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			//MY_LOGE(TAG, "%s OK", __FUNCTION__);
			status.error_flags &= ~ERR_EEPROM;//clear error flag
			break;
		} else if (ret == ESP_ERR_TIMEOUT) {
			MY_LOGW(TAG, "%s timeout", __FUNCTION__);
			statistics.eeprom_errors++;
			status.error_flags |= ERR_EEPROM;
		} else {
			MY_LOGW(TAG, "%s ERR", __FUNCTION__);
			statistics.eeprom_errors++;
			status.error_flags |= ERR_EEPROM;
		}
	}

	//MY_LOGI(TAG, "%s OK, ret=%d", __FUNCTION__, ret);
	i2c_unlock_mutex();
	return ret;
}
#else //24LC64
esp_err_t eeprom_page_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size) {
	esp_err_t ret = ESP_OK;

//	for(int i = 0; i < size; i++)
//		printf("eeprom_page_write: %d\n\r", data[i] );

	//MY_LOGI(TAG, "eeprom_page_write addr:0x%x size:%d block:%02x data:%02x %02x %02x %02x", eeaddress, size, (((eeaddress>>8)&0x7)<<1), *(data+0), *(data+1), *(data+2), *(data+3));
	if(size > EEPROM_PAGE_SIZE || eeaddress%EEPROM_PAGE_SIZE )
	{
		MY_LOGE(TAG, "%s ALIGNMENT EE address:%x or size:%x", __FUNCTION__, eeaddress, size);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	if(i2c_lock_mutex())
	{
		MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
		statistics.eeprom_errors++;
		return ESP_FAIL;
	}

	int retry = 5;

	while(retry--)
	{
		vTaskDelay(20/portTICK_PERIOD_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		//i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK_VAL); //control byte
		i2c_master_write_byte(cmd, (deviceaddress << 1) | EEPROM_WRITE_ADDR, ACK_VAL);//control byte
		i2c_master_write_byte(cmd, eeaddress>>8, ACK_VAL); //address high byte
		i2c_master_write_byte(cmd, eeaddress&0xff, ACK_VAL); //address
		for(int i = 0; i < size; i++)
			i2c_master_write_byte(cmd, *(data+i), ACK_VAL); //data write
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			//MY_LOGE(TAG, "%s OK", __FUNCTION__);
			break;
		} else if (ret == ESP_ERR_TIMEOUT) {
			MY_LOGW(TAG, "%s timeout", __FUNCTION__);
			statistics.eeprom_errors++;
		} else {
			MY_LOGW(TAG, "%s ERR: %d", __FUNCTION__, ret);
			statistics.eeprom_errors++;
		}
	}

    i2c_unlock_mutex();
    vTaskDelay(20/portTICK_PERIOD_MS);
    return ret;
}
#endif //24LC64

esp_err_t EE_write_status()
{
	esp_err_t ret;

	ee_status.crc = CRC16((unsigned char *)&ee_status, EE_STATUS_SIZE - 2);
	ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_status, EE_STATUS_SIZE, ESP_LOG_INFO);
	ret = eeprom_page_write(EEPROM_I2C_ADDR, EE_STATUS_ADDR, (uint8_t*)&ee_status, EE_STATUS_SIZE);
	if(ret != ESP_OK)
	{
		MY_LOGE(TAG, "EE_write_status failed");
		status.error_flags |= ERR_EEPROM;
		statistics.eeprom_errors++;
	}
	else
		status.error_flags &= ~ERR_EEPROM;//clear error flag

	return ret;
}

esp_err_t EE_write_energy_status(int i)
{
	if((i > 0 && eeprom_size < 64) || eeprom_size == 0)
		return ESP_FAIL;
	ee_energy_status[i].timestamp = status.timestamp; //update timestamp
	int ret = eeprom_page_write(EEPROM_I2C_ADDR, EE_ENERGY_STATUS_ADDR + i*EE_RECORDER_SIZE, (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE);
	if(ret != ESP_OK)
	{
		MY_LOGE(TAG, "EE_write_energy_status failed");
		statistics.eeprom_errors++;
		status.error_flags |= ERR_EEPROM;
	}
	else
		status.error_flags &= ~ERR_EEPROM;//clear error flag

	return ret;
}

esp_err_t EE_write_energy_page(int addr, uint8_t *values)
{
	//uint8_t buf = value;
	//for(int i = 0; i < 4; i++)
		//printf("Write Energy day %d: %d\n\r", i, values.value[i] );

	int ret = eeprom_page_write(EEPROM_I2C_ADDR, addr, values, 16);
	if(ret != ESP_OK)
	{
		MY_LOGE(TAG, "EE_write_energy_day failed");
		//statistics.eeprom_errors++;
		status.error_flags |= ERR_EEPROM;
	}
	else
		status.error_flags &= ~ERR_EEPROM;

	return ret;
}

char *EE_read_energy_day_records_JSON(int recorder)
{
	char *string = NULL;
	cJSON *days = NULL;
	cJSON *months = NULL;
	cJSON *years = NULL;
	int day_samples = 0;
	int month_samples = 0;
	int year_samples = 0;
	char temp_str[30];
	int i;

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	days = cJSON_CreateArray();
	if (days == NULL)
	{
		goto end;
	}

	months = cJSON_CreateArray();
	if (months == NULL)
	{
		goto end;
	}

	years = cJSON_CreateArray();
	if (years == NULL)
	{
		goto end;
	}

	energyCounterValuesStruct_t counter_values;
	int ret = getEnergyCounterValue(status.modbus_device[0].uart_port, status.modbus_device[0].address, &counter_values);
	if(ret)
	{
		MY_LOGW(TAG, "getEnergyCounterValue Failed");
		memset((void *)&counter_values, 0, sizeof(counter_values));
		//goto end;
	}

	//for(i = 0; i < 4; i++)
		//ESP_LOGI(TAG, "Counter: %d value: %.1f",i+1, counter_values.value[i]/10.0);

	print_local_time(ee_energy_status[recorder].timestamp, temp_str);
	cJSON_AddItemToObject(json_root, "recorder", cJSON_CreateNumber(recorder + 1));
	cJSON_AddItemToObject(json_root, "no_recorders", cJSON_CreateNumber(eeprom_size/16)); //16k = 1 recorder, 64k = 4 recorders
	cJSON_AddItemToObject(json_root, "last_record_time", cJSON_CreateString(temp_str));
	cJSON_AddItemToObject(json_root, "last_record_timestamp", cJSON_CreateNumber(ee_energy_status[recorder].timestamp));
	cJSON_AddItemToObject(json_root, "local_time", cJSON_CreateString(status.local_time));
	sprintf(temp_str, "%.1f", counter_values.value[settings.energy_log[recorder] - 1]/10.0); //to kWh
	cJSON_AddItemToObject(json_root, "current_value", cJSON_CreateString(temp_str));
	cJSON_AddItemToObject(json_root, "unit", cJSON_CreateString("kWh"));
	cJSON_AddItemToObject(json_root, "day_position", cJSON_CreateNumber(ee_energy_status[recorder].energy_day_wp - 1));
	cJSON_AddItemToObject(json_root, "month_position", cJSON_CreateNumber(ee_energy_status[recorder].energy_month_wp - 1));
	cJSON_AddItemToObject(json_root, "year_position", cJSON_CreateNumber(ee_energy_status[recorder].energy_year_wp - 1));
	cJSON_AddItemToObject(json_root, "counter_number", cJSON_CreateNumber(settings.energy_log[recorder]));

	cJSON_AddItemToObject(json_root, "days", days);

	//energy values from eeprom
	uint8_t *ee_days_buf;
	ee_days_buf = malloc(EEPROM_PAGE_SIZE * EE_ENERGY_DAYS_PAGES);

	for(i = 0; i < EE_ENERGY_DAYS_PAGES; i++)
	{
		//printf("reading page %d: to %d\n\r", i, *ee_days_buf + i*EEPROM_PAGE_SIZE);
		eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_DAYS_ADDR + recorder*EE_RECORDER_SIZE + i*EEPROM_PAGE_SIZE, ee_days_buf + i*EEPROM_PAGE_SIZE, EEPROM_PAGE_SIZE);
		//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status, EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);
	}

	//EEenergyDayStruct_t *ee_days = (EEenergyDayStruct_t*)ee_days_buf;
	uint32_t *ee_days = (uint32_t *)ee_days_buf;

	for(i = 0; i < EE_ENERGY_DAYS_NUMBER; i++) //((EE_ENERGY_MONTHS_ADDR-EE_ENERGY_DAYS_ADDR)/4)
	{
		//char str_result[10];
		//printf("%d: %d,", i, ee_days->value[i]);
		if(*(ee_days+i) == 0xffffffff)
			*(ee_days+i) = 0;

		if(*(ee_days+i) == 0)
			continue; //skip empty data

		day_samples++;
		sprintf(temp_str, "%.*f", 1, *(ee_days+i)/10.0); //to kWh
		//printf("%d: %s,", i, temp_str);
		cJSON_AddItemToArray(days, cJSON_CreateString(temp_str));
	}

	//printf("\n\r");
	free(ee_days_buf);

	cJSON_AddItemToObject(json_root, "months", months);

	//energy values from eeprom
	uint8_t *ee_months_buf;
	ee_months_buf = malloc(EEPROM_PAGE_SIZE * EE_ENERGY_MONTHS_PAGES);

	for(i = 0; i < EE_ENERGY_MONTHS_PAGES; i++)
	{
		//printf("reading page %d: to %d\n\r", i, *ee_days_buf + i*EEPROM_PAGE_SIZE);
		eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_MONTHS_ADDR + recorder*EE_RECORDER_SIZE + i*EEPROM_PAGE_SIZE, ee_months_buf + i*EEPROM_PAGE_SIZE, EEPROM_PAGE_SIZE);
		//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status, EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);
	}

	//EEenergyDayStruct_t *ee_days = (EEenergyDayStruct_t*)ee_days_buf;
	uint32_t *ee_months = (uint32_t *)ee_months_buf;

	for(i = 0; i < EE_ENERGY_MONTHS_NUMBER; i++)
	{
		//char str_result[10];
		//printf("%d: %d,", i, ee_days->value[i]);
		if(*(ee_months+i) == 0xffffffff)
			*(ee_months+i) = 0;

		if(*(ee_months+i) == 0)
			continue; //skip empty data

		month_samples++;
		sprintf(temp_str, "%.*f", 1, *(ee_months+i)/10.0); //to kWh
		//printf("%d: %s,", i, temp_str);
		cJSON_AddItemToArray(months, cJSON_CreateString(temp_str));
	}

	//printf("\n\r");

	free(ee_months_buf);

	cJSON_AddItemToObject(json_root, "years", years);

	//energy values from eeprom
	uint8_t *ee_years_buf;
	ee_years_buf = malloc(EEPROM_PAGE_SIZE * EE_ENERGY_YEARS_PAGES);

	for(i = 0; i < EE_ENERGY_YEARS_PAGES; i++)
	{
		//printf("reading page %d: to %d\n\r", i, *ee_days_buf + i*EEPROM_PAGE_SIZE);
		eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_YEARS_ADDR + recorder*EE_RECORDER_SIZE + i*EEPROM_PAGE_SIZE, ee_years_buf + i*EEPROM_PAGE_SIZE, EEPROM_PAGE_SIZE);
		//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status, EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);
	}

	//EEenergyDayStruct_t *ee_days = (EEenergyDayStruct_t*)ee_days_buf;
	uint32_t *ee_years = (uint32_t *)ee_years_buf;

	for(i = 0; i < EE_ENERGY_YEARS_NUMBER; i++)
	{
		//char str_result[10];
		//printf("%d: %d,", i, ee_days->value[i]);
		if(*(ee_years+i) == 0xffffffff)
			*(ee_years+i) = 0;

		if(*(ee_years+i) == 0)
			continue; //skip empty data

		year_samples++;
		sprintf(temp_str, "%.*f", 1, *(ee_years+i)/10.0); //to kWh
		//printf("%d: %s,", i, temp_str);
		cJSON_AddItemToArray(years, cJSON_CreateString(temp_str));
	}

	//printf("\n\r");

	free(ee_years_buf);

	cJSON_AddItemToObject(json_root, "day_samples", cJSON_CreateNumber(day_samples));
	cJSON_AddItemToObject(json_root, "month_samples", cJSON_CreateNumber(month_samples));
	cJSON_AddItemToObject(json_root, "year_samples", cJSON_CreateNumber(year_samples));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print energy log JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

int ee_write_energy_value(int location, uint32_t value, int type, int recorder)
{
	uint32_t *ee_pnt;
	uint8_t ee_page_buf[EEPROM_PAGE_SIZE];
	int ret;
	int start_addr;
	ee_pnt = (uint32_t *)ee_page_buf;

	if((recorder > 0 && eeprom_size == 16) || eeprom_size == 0)
		return -1;

	if(type == DAY_ENERGY)
	{
		start_addr = EE_ENERGY_DAYS_ADDR;
		if(location > EE_ENERGY_DAYS_NUMBER - 1)
			location -= EE_ENERGY_DAYS_NUMBER;
	}
	else if(type == MONTH_ENERGY)
	{
		start_addr = EE_ENERGY_MONTHS_ADDR;
		if(location > EE_ENERGY_MONTHS_NUMBER - 1)
			location -= EE_ENERGY_MONTHS_NUMBER;
	}
	else if(type == YEAR_ENERGY)
	{
		start_addr = EE_ENERGY_YEARS_ADDR;
		if(location > EE_ENERGY_YEARS_NUMBER - 1)
			location -= EE_ENERGY_YEARS_NUMBER;
	}
	else
		return -1;

	ret = eeprom_read(EEPROM_I2C_ADDR, start_addr + recorder*EE_RECORDER_SIZE + (location/4)*EEPROM_PAGE_SIZE, ee_page_buf, EEPROM_PAGE_SIZE);
	if(ret != ESP_OK)
		return ret;
	//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status, EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);

	//write new value
	//printf("EEPROM energy write position: %d offset: %d, page: %d\n\r", location, location%4, location/4);
	*(ee_pnt +(location%4)) = value;

//	for(int i = 0; i < 4; i++)
//		printf("Writing %d: %d\n\r", (location/4)*4+i, *(ee_pnt+i) );

	int ee_address = start_addr + recorder*EE_RECORDER_SIZE  + ((location/4)*16);
	ret = EE_write_energy_page(ee_address, ee_page_buf);
	if(ret == ESP_OK)
	{
		MY_LOGI(TAG, "ee_write_energy_value loc:%d val:%d type:%d OK", location, value, type);
		if(type == DAY_ENERGY)
		{
			ee_energy_status[recorder].energy_day_wp = location + 1;
			if(ee_energy_status[recorder].energy_day_wp >= EE_ENERGY_DAYS_NUMBER)
				ee_energy_status[recorder].energy_day_wp = 0; //rollover
		}
		else if(type == MONTH_ENERGY)
		{
			ee_energy_status[recorder].energy_month_wp = location + 1;
			if(ee_energy_status[recorder].energy_month_wp >= EE_ENERGY_MONTHS_NUMBER)
				ee_energy_status[recorder].energy_month_wp = 0; //rollover
		}
		else if(type == YEAR_ENERGY)
		{
			ee_energy_status[recorder].energy_year_wp = location + 1;
			if(ee_energy_status[recorder].energy_year_wp >= EE_ENERGY_YEARS_NUMBER)
				ee_energy_status[recorder].energy_year_wp = 0; //rollover
		}

		EE_write_energy_status(recorder);
	}
	else
		MY_LOGE(TAG, "ee_write_energy_value loc:%d val:%d type:%d Failed", location, value, type);
	return ret;
}

void energy_counter_recorder(struct tm timeinfo)
{
	int ret;
	energyCounterValuesStruct_t counter_values;
	uint8_t ee_page_buf[EEPROM_PAGE_SIZE];
	uint8_t got_values = 0;

	for(int i = 0; i < NUMBER_OF_RECORDERS; i++)
	{
		uint8_t EE_write_energy_status_flag = 0;
		uint8_t first_record = 0;

		if(settings.energy_log[i] == 0)//recorder disabled
			continue;

		if((i > 0 && eeprom_size == 16) || eeprom_size == 0)
			continue; //only one recorder

		if(ee_energy_status[i].timestamp < 1600000000 || ee_energy_status[i].timestamp > 3000000000)//invalid timestamp
		{
			MY_LOGE(TAG, "invalid timestamp: %08x", ee_energy_status[i].timestamp);
			//reset all pointers
			ee_energy_status[i].energy_day_wp = 0;
			ee_energy_status[i].energy_month_wp = 0;
			ee_energy_status[i].energy_year_wp = 0;
			ee_energy_status[i].timestamp = status.timestamp;
		}

		if( status.timestamp < ee_energy_status[i].timestamp )
			continue; //skip record if time is in the past

		if(ee_energy_status[i].energy_day_wp == 0 && ee_energy_status[i].energy_month_wp == 0 && ee_energy_status[i].energy_year_wp == 0)
			first_record++;

		if((timeinfo.tm_mday) != getDayFromTime(ee_energy_status[i].timestamp) || first_record)
		{
			ret = getEnergyCounterValue(status.modbus_device[0].uart_port, status.modbus_device[0].address, &counter_values);
			if(ret)
				continue;
			else
				got_values = 1;

			//		printf("Energy counters:\n\r");
			//		for(int i = 0; i < 4; i++)
			//			printf("Counter %d: %d\n\r", i + 1, counter_values.value[i]);

			//read energy page from eeprom
			uint32_t *ee_days;
			ee_days = (uint32_t *)ee_page_buf;

			ret = eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_DAYS_ADDR + i*EE_RECORDER_SIZE + (ee_energy_status[i].energy_day_wp/4)*EEPROM_PAGE_SIZE, ee_page_buf, EEPROM_PAGE_SIZE);
			if(ret != ESP_OK)
				continue;
			//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);

			//write new value
			//printf("EEPROM energy write position: %d offset: %d, page: %d counter: %d\n\r", ee_energy_status[i].energy_day_wp, ee_energy_status[i].energy_day_wp%4, ee_energy_status[i].energy_day_wp/4, settings.energy_log[i]);
			*(ee_days +(ee_energy_status[i].energy_day_wp%4)) = counter_values.value[settings.energy_log[i] - 1];//counter is: settings.energy_log[i] - 1

			//		for(int i = 0; i < 4; i++)
			//			printf("Writing day %d: %d\n\r", (ee_energy_status[i].energy_day_wp/4)*4+i, *(ee_days+i) );

			int ee_address = EE_ENERGY_DAYS_ADDR+ i*EE_RECORDER_SIZE + ((ee_energy_status[i].energy_day_wp/4)*16);
			ret = EE_write_energy_page(ee_address, ee_page_buf);
			if(ret == ESP_OK)
			{
				ee_energy_status[i].energy_day_wp++;
				if(ee_energy_status[i].energy_day_wp >= EE_ENERGY_DAYS_NUMBER)
					ee_energy_status[i].energy_day_wp = 0; //rollover
				EE_write_energy_status_flag++; //EE_write_energy_status(i);
			}
		}
		if((timeinfo.tm_mon) != getMonthFromTime(ee_energy_status[i].timestamp) || first_record)
		{
			if(got_values == 0)
			{
				ret = getEnergyCounterValue(status.modbus_device[0].uart_port, status.modbus_device[0].address, &counter_values);
				if(ret)
					continue;
				else
					got_values = 1;
			}
			//read energy page from eeprom
			uint32_t *ee_months = (uint32_t *)ee_page_buf;
			ret = eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_MONTHS_ADDR + i*EE_RECORDER_SIZE + (ee_energy_status[i].energy_month_wp/4)*EEPROM_PAGE_SIZE, ee_page_buf, EEPROM_PAGE_SIZE);
			if(ret != ESP_OK)
				continue;
			//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);

			//write new value
			//printf("EEPROM energy write position: %d offset: %d, page: %d\n\r", ee_energy_status[i].energy_month_wp, ee_energy_status[i].energy_month_wp%4, ee_energy_status[i].energy_month_wp/4);
			*(ee_months +(ee_energy_status[i].energy_month_wp%4)) = counter_values.value[settings.energy_log[i] - 1];//todo

			//		for(int i = 0; i < 4; i++)
			//			printf("Writing day %d: %d\n\r", (ee_energy_status[i].energy_month_wp/4)*4+i, *(ee_months + i) );

			int ee_address = EE_ENERGY_MONTHS_ADDR + i*EE_RECORDER_SIZE +((ee_energy_status[i].energy_month_wp/4)*16);
			ret = EE_write_energy_page(ee_address, ee_page_buf);
			if(ret == ESP_OK)
			{
				ee_energy_status[i].energy_month_wp++;
				if(ee_energy_status[i].energy_month_wp >= EE_ENERGY_MONTHS_NUMBER)
					ee_energy_status[i].energy_month_wp = 0; //rollover
				EE_write_energy_status_flag++; //EE_write_energy_status(i);
			}
		}
		if((timeinfo.tm_year) > getYearFromTime(ee_energy_status[i].timestamp) || first_record)
		{
			if(got_values == 0)
			{
				ret = getEnergyCounterValue(status.modbus_device[0].uart_port, status.modbus_device[0].address, &counter_values);
				if(ret)
					continue;
				else
					got_values = 1;
			}

			//read energy page from eeprom
			uint32_t *ee_years = (uint32_t *)ee_page_buf;

			ret = eeprom_read(EEPROM_I2C_ADDR, EE_ENERGY_YEARS_ADDR + i*EE_RECORDER_SIZE + (ee_energy_status[i].energy_year_wp/4)*EEPROM_PAGE_SIZE, ee_page_buf, EEPROM_PAGE_SIZE);
			if(ret != ESP_OK)
				continue;
			//ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)&ee_energy_status[i], EE_ENERGY_STATUS_SIZE, ESP_LOG_INFO);

			//write new value
			//printf("EEPROM energy write position: %d offset: %d, page: %d\n\r", ee_energy_status[i].energy_year_wp, ee_energy_status[i].energy_year_wp%4, ee_energy_status[i].energy_year_wp/4);
			*(ee_years +(ee_energy_status[i].energy_year_wp%4)) = counter_values.value[settings.energy_log[i] - 1];

			//		for(int i = 0; i < 4; i++)
			//			printf("Writing day %d: %d\n\r", (ee_energy_status[i].energy_year_wp/4)*4+i, *(ee_years+i) );

			int ee_address = EE_ENERGY_YEARS_ADDR + i*EE_RECORDER_SIZE +((ee_energy_status[i].energy_year_wp/4)*16);
			ret = EE_write_energy_page(ee_address, ee_page_buf);
			if(ret == ESP_OK)
			{
				ee_energy_status[i].energy_year_wp++;
				if(ee_energy_status[i].energy_year_wp >= EE_ENERGY_YEARS_NUMBER)
					ee_energy_status[i].energy_year_wp = 0; //rollover
				EE_write_energy_status_flag++; //EE_write_energy_status(i);
			}
		}
		if(EE_write_energy_status_flag)
			EE_write_energy_status(i);
	}//for recorders
}

void erase_energy_recorder(int recorder_mask)
{
	int i;
	int recorder;
	uint8_t ee_buf[EEPROM_PAGE_SIZE];

	MY_LOGI(TAG, "recorder_mask: %02x", recorder_mask);

	for(recorder = 0; recorder < NUMBER_OF_RECORDERS; recorder++)
	{
		if((recorder > 0 && eeprom_size == 16) || eeprom_size == 0) //eeprom size 16 has 1 recorder
			return;

		if(settings.energy_log[recorder])
			continue; //not disabled

		if( !((1 << recorder) & recorder_mask) ) //check mask
			continue;

		memset((void*)ee_buf, 0, EEPROM_PAGE_SIZE);//empty page

		for(i = 0; i < EE_ENERGY_DAYS_PAGES; i++)
		{
			EE_write_energy_page(EE_ENERGY_DAYS_ADDR + recorder*EE_RECORDER_SIZE + i*16, ee_buf);
		}
		ee_energy_status[recorder].energy_day_wp = 0;

		for(i = 0; i < EE_ENERGY_MONTHS_PAGES; i++)
		{
			EE_write_energy_page(EE_ENERGY_MONTHS_ADDR + recorder*EE_RECORDER_SIZE + i*16, ee_buf);
		}
		ee_energy_status[recorder].energy_month_wp = 0;

		for(i = 0; i < EE_ENERGY_YEARS_PAGES; i++)
		{
			EE_write_energy_page(EE_ENERGY_YEARS_ADDR + recorder*EE_RECORDER_SIZE  + i*16, ee_buf);
		}
		ee_energy_status[recorder].energy_year_wp = 0;

		EE_write_energy_status(recorder);
	}
}

/* just for test */
void set_energy_wp(int recorder, int day, int month, int year)
{
	if((recorder > 0 && eeprom_size == 16) || eeprom_size == 0)
		return;

	ee_energy_status[recorder].energy_day_wp = day;
	ee_energy_status[recorder].energy_month_wp = month;
	ee_energy_status[recorder].energy_year_wp = year;
	EE_write_energy_status(recorder);
}

void test_write_energy()
{
	//days
	for(int i = 0; i < EE_ENERGY_DAYS_NUMBER+100; i++)
		ee_write_energy_value(i, i*1000, 0, 0);

	//months
	for(int i = 0; i < EE_ENERGY_MONTHS_NUMBER+5; i++)
		ee_write_energy_value(i, i*2000, 1, 0);

	//months
	for(int i = 0; i < EE_ENERGY_YEARS_NUMBER+5; i++)
		ee_write_energy_value(i, i*3000, 2, 0);
}

/* i2c semaphore */
int i2c_lock_mutex()
{
    if (!xSemaphoreTake(i2c_Semaphore, ( TickType_t )I2C_SEMAPHORE_TIMEOUT_ERROR))
            return I2C_SEMAPHORE_TIMEOUT_ERROR;
    else
        return 0;
}

int i2c_unlock_mutex()
{
    if (!xSemaphoreGive(i2c_Semaphore))
        return -1;
    else
        return 0;
}
/**************************/

#if 0

esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte) {
    esp_err_t ret;
    int retry = 5;

    if(i2c_lock_mutex())
    {
    	MY_LOGE(TAG, "%s i2c semaphore timeout", __FUNCTION__);
    	statistics.eeprom_errors++;
    	return ESP_FAIL;
    }

    while(retry--)
    {
    	vTaskDelay(50/portTICK_PERIOD_MS);
    	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    	i2c_master_start(cmd);
    	i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK_VAL); //control byte
    	i2c_master_write_byte(cmd, eeaddress&0xff, ACK_VAL); //address
    	i2c_master_write_byte(cmd, byte, ACK_VAL); //data write
    	i2c_master_stop(cmd);
    	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_RATE_MS);
    	i2c_cmd_link_delete(cmd);
    	if (ret == ESP_OK) {
    		//MY_LOGE(TAG, "%s OK\n\r", __FUNCTION__);
    		break;
    	} else if (ret == ESP_ERR_TIMEOUT) {
    		MY_LOGE(TAG, "%s timeout\n\r", __FUNCTION__);
    		statistics.eeprom_errors++;
    	} else {
    		MY_LOGE(TAG, "%s ERR\n\r", __FUNCTION__);
    		statistics.eeprom_errors++;
    	}
    }

    i2c_unlock_mutex();

    return ret;
}

//24LC64
esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte) {
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)|EEPROM_WRITE_ADDR, 1);
    i2c_master_write_byte(cmd, eeaddress>>8, 1);
    i2c_master_write_byte(cmd, eeaddress&0xFF, 1);
    i2c_master_write_byte(cmd, byte, 1);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data) {
	esp_err_t ret;
	//uint8_t data;
	int retry = 5;

	while(retry--)
	{
		vTaskDelay(50/portTICK_PERIOD_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK_VAL); //control byte
		i2c_master_write_byte(cmd, eeaddress&0xFF, ACK_VAL); //address
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1)| EEPROM_READ_ADDR, ACK_VAL); //control byte
		i2c_master_read_byte(cmd, data, NACK_VAL);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			//MY_LOGE(TAG, "%s OK\n\r", __FUNCTION__);
			break;
		} else if (ret == ESP_ERR_TIMEOUT) {
			MY_LOGE(TAG, "%s timeout\n\r", __FUNCTION__);
		} else {
			MY_LOGE(TAG, "%s ERR\n\r", __FUNCTION__);
		}
	}
    return ret;
}

esp_err_t eeprom_write_long(uint8_t deviceaddress, uint16_t eeaddress, uint32_t data) {
	for(int i = 0; i < 4; i++)
	{
		eeprom_write_byte(deviceaddress, eeaddress+i, (uint8_t)(data>>(i*8)));
	}

	return ESP_OK;
}

uint32_t eeprom_read_long(uint8_t deviceaddress, uint16_t eeaddress) {
	uint8_t tmp[4];

	for(int i = 0; i < 4; i++)
		eeprom_read_byte(deviceaddress, eeaddress+i, &tmp[i]);

	return (tmp[3]<<24 | tmp[2]<<16 | tmp[1]<<8 | tmp[0]);
}

esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte) {
    esp_err_t ret;

    //MY_LOGE(TAG, "eeprom_write_byte addr:0x%x data:0x%02x block:%02x\n\r", eeaddress, byte, (((eeaddress>>8)&0x7)<<1));
    vTaskDelay(30/portTICK_PERIOD_MS);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //control byte
    i2c_master_write_byte(cmd, eeaddress&0xff, ACK); //address
    i2c_master_write_byte(cmd, byte, ACK); //data write
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t eeprom_page_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size)
{
	MY_LOGE(TAG, "eeprom_page_write addr:0x%x size:%d block:%02x data:%02x %02x %02x %02x\n\r", eeaddress, size, (((eeaddress>>8)&0x7)<<1), *(data+0), *(data+1), *(data+2), *(data+3));
	if(size > EEPROM_PAGE_SIZE || eeaddress%EEPROM_PAGE_SIZE )
		return ESP_FAIL;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //control byte
	i2c_master_write_byte(cmd, eeaddress&0xff, ACK); //address
	for(int i = 0; i < size; i++)
		i2c_master_write_byte(cmd, *(data+i), ACK); //data write
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}



uint8_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress) {

	vTaskDelay(30/portTICK_PERIOD_MS);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //control byte
    i2c_master_write_byte(cmd, eeaddress&0xFF, ACK); //address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1)| EEPROM_READ_ADDR, ACK); //control byte

    uint8_t data;
    i2c_master_read_byte(cmd, &data, NO_ACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    //MY_LOGE(TAG, "eeprom_read_byte addr:0x%x data:0x%02x block:%02x\n\r", eeaddress, data, (((eeaddress>>8)&0x7)<<1));
    return data;
}
#endif

#if 0
static esp_err_t i2c_master_init(void)
{
#if 0
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);

    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    i2c_master_driver_initialize();
#endif
    return 0;
}
#endif

#if 0 //todo: sequential write does not work
esp_err_t eeprom_page_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size) {
    esp_err_t ret = ESP_OK;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //use 3 address bits for block select
    //i2c_master_write_byte(cmd, eeaddress&0xff, ACK);

    int bytes_remaining = size;
    int current_address = eeaddress;
    int first_write_size = EEPROM_PAGE_SIZE - (eeaddress % EEPROM_PAGE_SIZE);
    MY_LOGE(TAG, "addr:0x%x size:%d First write size:%d\n\r", eeaddress, size, first_write_size);
    if (bytes_remaining <= first_write_size) {
        i2c_master_write(cmd, data, bytes_remaining, 1);
    } else {
        i2c_master_write(cmd, data, first_write_size, 1);
        bytes_remaining -= first_write_size;
        current_address += first_write_size;
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        if (ret != ESP_OK) return ret;
        while (bytes_remaining > 0) {
            cmd = i2c_cmd_link_create();
            MY_LOGE(TAG, "current_address:%d bytes_remaining:%d\n\r", current_address, bytes_remaining);
            // 2ms delay period to allow EEPROM to write the page
            // buffer to memory.
            vTaskDelay(20/portTICK_PERIOD_MS);

            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //use 3 address bits for block select
            i2c_master_write_byte(cmd, eeaddress&0xff, ACK);
            if (bytes_remaining <= EEPROM_PAGE_SIZE) {
                i2c_master_write(cmd, data+(size-bytes_remaining), bytes_remaining, 1);
                bytes_remaining = 0;
            } else {
                i2c_master_write(cmd, data+(size-bytes_remaining), EEPROM_PAGE_SIZE, 1);
                bytes_remaining -= EEPROM_PAGE_SIZE;
                current_address += EEPROM_PAGE_SIZE;
            }
            i2c_master_stop(cmd);
            ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            if (ret != ESP_OK) return ret;
        }
    }

    return ret;
}
#endif

#if 0 //todo: sequential read does not work
esp_err_t eeprom_read(uint8_t deviceaddress, uint16_t eeaddress, uint8_t *data, size_t size) {
	MY_LOGE(TAG, "	eeprom_read at address 0x%04X size:%d\n", eeaddress, size);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1) | EEPROM_WRITE_ADDR, ACK); //control byte
	i2c_master_write_byte(cmd, eeaddress&0xFF, ACK); //address
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (deviceaddress<<1)| (((eeaddress>>8)&0x7)<<1)| EEPROM_READ_ADDR, ACK); //control byte

    /*if (size > 1) {
        i2c_master_read(cmd, data, size-1, ACK);
    }*/
	for(uint8_t i = 0; i < size-1;i++)
	{
		i2c_master_read_byte(cmd, data+i, ACK);
	}

    i2c_master_read_byte(cmd, data+size-1, NO_ACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
#endif

#if 0


void eeprom_task(void *arg) {
    //const uint8_t eeprom_address = EEPROM_I2C_ADDR;

    i2c_driver_install(i2c_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    i2c_master_driver_initialize();

    //do_i2cdetect_cmd();

    //vTaskDelay(1000/portTICK_PERIOD_MS);

    //eeprom_write_long(eeprom_address, EE_POWER_UP_COUNTER_LOCATION, 0);

    status.power_up_counter = eeprom_read_long(EEPROM_I2C_ADDR, EE_POWER_UP_COUNTER_LOCATION);
    MY_LOGE(TAG, "Power up counter: %04x\n\r", status.power_up_counter);
    vTaskDelay(100/portTICK_PERIOD_MS);
    status.power_up_counter++;
    eeprom_write_long(EEPROM_I2C_ADDR, EE_POWER_UP_COUNTER_LOCATION, status.power_up_counter);
    vTaskDelay(100/portTICK_PERIOD_MS);
    MY_LOGE(TAG, "Wrote 0x%02x read: %02x\n\r", status.power_up_counter, eeprom_read_long(EEPROM_I2C_ADDR, EE_POWER_UP_COUNTER_LOCATION));

#if 0
    // EEPROM single byte write example
    MY_LOGE(TAG, "Single byte write:\n\r");
    for(int i = 0; i < 11; i++)
    {
    	eeprom_write_byte(eeprom_address, starting_address<<i, (i<8?single_write_byte<<i:single_write_byte<<(i-8))+2);
    	MY_LOGE(TAG, "	Wrote byte 0x%02X to address 0x%04X\n", (i<8?single_write_byte<<i:single_write_byte<<(i-8))+2, starting_address<<i);
    	vTaskDelay(100/portTICK_PERIOD_MS);
    }

    // EEPROM random read example
    MY_LOGE(TAG, "Single byte read:\n\r");
    for(int i = 0; i < 11; i++)
    {
    	uint8_t random_read_byte = eeprom_read_byte(eeprom_address, starting_address<<i);
    	//eeprom_read_byte(eeprom_address, starting_address<<i);
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", random_read_byte, starting_address<<i);
    	vTaskDelay(100/portTICK_PERIOD_MS);
    }
#endif

#if 0
    // EEPROM sequential read example
    uint16_t starting_address = 0x400;
    MY_LOGE(TAG, "Sequential write (16) bytes:\n\r");
    //uint8_t* sequential_read_data = (uint8_t*) calloc(strlen(page_write_data)+1, 1);
    //uint8_t sequential_read_data[16];

    uint8_t write_data[16];
    for(int i = 0; i < 16; i++)
    {
    	write_data[i] = 0xf - i;
    	MY_LOGE(TAG, "	Wrote byte 0x%02X to address 0x%04X\n", write_data[i], starting_address+i);
    	eeprom_write_byte(eeprom_address, starting_address+i, write_data[i]);
    	vTaskDelay(30/portTICK_PERIOD_MS);
    }
    //eeprom_write(eeprom_address, starting_address, (uint8_t*)write_data, 16);
    vTaskDelay(30/portTICK_PERIOD_MS);

    // EEPROM sequential read example and error checking
    MY_LOGE(TAG, "Sequential read (%d) bytes:\n\r", sizeof(write_data));
    uint8_t* multibyte_read_data = (uint8_t*) calloc(sizeof(write_data)+1, 1);
    esp_err_t ret = eeprom_read(eeprom_address, starting_address, multibyte_read_data, sizeof(write_data));

    if (ret == ESP_ERR_TIMEOUT) MY_LOGE(TAG, "I2C timeout...\n");
    if (ret == ESP_OK) MY_LOGE(TAG, "The read operation was successful!\n");
    else MY_LOGE(TAG, "The read operation was not successful, no ACK recieved.  Is the device connected properly?\n");

    for(int i = 0; i < 16; i++)
    {
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", multibyte_read_data[i], starting_address+i);
    }

    free(multibyte_read_data);

    MY_LOGE(TAG, "---- Single byte read (%d) bytes from: 0x%x\n\r", 16, starting_address);
    for(int i = 0; i < 16; i++)
    {
    	uint8_t random_read_byte = eeprom_read_byte(eeprom_address, starting_address+i);
    	eeprom_read_byte(eeprom_address, starting_address+i);
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", random_read_byte, starting_address+i);
    	vTaskDelay(30/portTICK_PERIOD_MS);
    }
#endif

#if 0
    MY_LOGE(TAG, "---- Single byte write to 0x%x:\n\r", starting_address);
    for(int i = 0; i < 16; i++)
    {
    	eeprom_write_byte(eeprom_address, starting_address+i, single_write_byte+ 0x9f - i);
    	MY_LOGE(TAG, "	Wrote byte 0x%02X to address 0x%04X\n", single_write_byte+ 0xf - i, starting_address+i);
    	vTaskDelay(100/portTICK_PERIOD_MS);
    }

    MY_LOGE(TAG, "---- Sequential read (16) bytes from: 0x%x\n\r", starting_address);
    uint8_t sequential_read_data[16];
    esp_err_t ret = eeprom_read(eeprom_address, starting_address, sequential_read_data, 16);
    if (ret == ESP_OK) MY_LOGE(TAG, "The read operation was successful!\n");
    else MY_LOGE(TAG, "The read operation was not successful\n");

    for(int i = 0; i < 16; i++)
    {
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", sequential_read_data[i], starting_address+i);
    }
    vTaskDelay(30/portTICK_PERIOD_MS);

    MY_LOGE(TAG, "---- Single byte read (%d) bytes from: 0x%x\n\r", 16, starting_address);
    for(int i = 0; i < 16; i++)
    {
    	uint8_t random_read_byte = eeprom_read_byte(eeprom_address, starting_address+i);
    	eeprom_read_byte(eeprom_address, starting_address+i);
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", random_read_byte, starting_address+i);
    	vTaskDelay(30/portTICK_PERIOD_MS);
    }
#endif

#if 0
    starting_address = 0x200;
    // EEPROM page write example
    char* page_write_data = "EEPROM page writing allows long strings of text to be written quickly, and with a lot less write cycles!\0";
    MY_LOGE(TAG, "Sequential write (%d) bytes:\n\r", strlen(page_write_data));
    //char* page_write_data = "EEPROM";
    eeprom_write(eeprom_address, starting_address, (uint8_t*)page_write_data, strlen(page_write_data));
    MY_LOGE(TAG, "Wrote the following string to EEPROM: %s\n", page_write_data);

    vTaskDelay(30/portTICK_PERIOD_MS);

    // EEPROM sequential read example and error checking
    MY_LOGE(TAG, "Sequential read (%d) bytes:\n\r", strlen(page_write_data));
    uint8_t* multibyte_read_data = (uint8_t*) calloc(strlen(page_write_data)+1, 1);
    esp_err_t ret = eeprom_read(eeprom_address, starting_address, multibyte_read_data, strlen(page_write_data)+1);

    if (ret == ESP_ERR_TIMEOUT) MY_LOGE(TAG, "I2C timeout...\n");
    if (ret == ESP_OK) MY_LOGE(TAG, "The read operation was successful!\n");
    else MY_LOGE(TAG, "The read operation was not successful, no ACK recieved.  Is the device connected properly?\n");

    MY_LOGE(TAG, "Read the following string from EEPROM: %s\n", (char*)multibyte_read_data);

    MY_LOGE(TAG, "Single byte read (%d) bytes:\n\r", strlen(page_write_data));
    for(int i = 0; i < strlen(page_write_data); i++)
    {
    	uint8_t random_read_byte = eeprom_read_byte(eeprom_address, starting_address+i);
    	eeprom_read_byte(eeprom_address, starting_address+i);
    	MY_LOGE(TAG, "	Read byte 0x%02X at address 0x%04X\n", random_read_byte, starting_address+i);
    	vTaskDelay(30/portTICK_PERIOD_MS);
    }

    free(multibyte_read_data);
#endif

    i2c_driver_delete(I2C_MASTER_NUM);

    // We're done with this task now.  Deallocate so the computer doesn't complain
    vTaskDelete(NULL);
}
#endif

#if 0
// Run on APP CPU of ESP32 microcontroller
void EEPROM_STORAGE() {
	i2c_master_init();
    xTaskCreate(&eeprom_task, "eeprom_read_write_demo", 1024 * 2, NULL, 5, NULL);
}
#endif

#endif //#ifdef EEPROM_STORAGE
