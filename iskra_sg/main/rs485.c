/* Uart Events Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef ENABLE_RS485_PORT

extern u8_t tcp_obuf[RS485_RX_BUF_SIZE];

static const char *TAG = "RS485_APP";
static QueueHandle_t RS485_QUEUE;

#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
static void rs485_rts2_pin_init()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_UART2_RTS_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}
#endif //#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY

#if CONFIG_BOARD_SG
void rs485_tx_enable_pin_init()
#define GPIO_485_TX_EN_PIN  (1ULL<<(GPIO_NUM_2))
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_485_TX_EN_PIN;
	//disable pull-down mode
	io_conf.pull_down_en = 1;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
}
#endif //#if CONFIG_BOARD_SG

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    //size_t buffered_size;
    //uint8_t* dtmp = (uint8_t*) malloc(100);
    for(;;) {
        //Waiting for UART2 event.
        if(xQueueReceive(RS485_QUEUE, (void * )&event, (portTickType)portMAX_DELAY)) {
            //bzero(dtmp, RD_BUF_SIZE);
            //MY_LOGI(TAG, "uart[%d] event:", RS485_UART_PORT);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    xEventGroupSetBits(uart_event_group, RS485_DATA_READY_BIT);
#ifdef SERIAL_CIRCULAR_BUFFER
                    uart_read_bytes(RS485_UART_PORT, &serBuff[0][RS485_UART_PORT]+serBuffWrPnt[RS485_UART_PORT], event.size, portMAX_DELAY);
                    serBuffWrPnt[RS485_UART_PORT] += event.size;
                    serBuffWrPnt[RS485_UART_PORT] &= (SER_BUFF_SIZE - 1); //circular buffer
#endif
                    //MY_LOGI(TAG, "[UART DATA]: %d", event.size);
                    //uart_read_bytes(RS485_UART_PORT, dtmp, event.size, portMAX_DELAY);
                    //MY_LOGI(TAG, "[DATA EVT]:%c", dtmp[0]);
                    //uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    MY_LOGW(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(RS485_UART_PORT);
                    xQueueReset(RS485_QUEUE);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    MY_LOGW(TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(RS485_UART_PORT);
                    xQueueReset(RS485_QUEUE);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                	MY_LOGW(TAG, "uart rx break");
                	break;
                	//Event of UART RX break detected
                case UART_DATA_BREAK:
                	MY_LOGW(TAG, "uart data break");
                	break;
					//Event of UART parity check error
                case UART_PARITY_ERR:
                    MY_LOGW(TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    MY_LOGW(TAG, "uart frame error");
                    //uart_flush_input(RS485_UART_PORT);
                    //xQueueReset(RS485_QUEUE);
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    MY_LOGW(TAG, "uart pattern detected");
#if 0                
                    uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(EX_UART_NUM);
                    MY_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(EX_UART_NUM);
                    } else {
                        uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                        MY_LOGI(TAG, "read data: %s", dtmp);
                        MY_LOGI(TAG, "read pat : %s", pat);
                    }
#endif                    
                    break;
                //Others
                default:
                    MY_LOGW(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    //free(dtmp);
    //dtmp = NULL;
    vTaskDelete(NULL);
}

static void rs485_init()
{
	uint32_t baud_rate_table[] ={1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
	uint16_t parity_table[]    ={UART_PARITY_DISABLE, UART_PARITY_ODD, UART_PARITY_EVEN};
	uint16_t stop_bits_table[] ={UART_STOP_BITS_1, UART_STOP_BITS_2};

	uart_config_t uart_config = {
			.baud_rate = baud_rate_table[settings.rs485_baud_rate],  //.baud_rate = RS485_BAUD_RATE,
			.data_bits = UART_DATA_8_BITS,
			.parity = parity_table[settings.rs485_parity],//UART_PARITY_DISABLE,
			.stop_bits = stop_bits_table[settings.rs485_stop_bits],//UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
	};
    
    //rs485_event_group = xEventGroupCreate();
    
#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
    rs485_rts2_pin_init();
#endif //#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY

    // Set UART log level
    //MY_LOG_level_set(TAG, MY_LOG_INFO);
    
    MY_LOGI(TAG, "RS485 Init");

    // Configure UART parameters
    uart_param_config(RS485_UART_PORT, &uart_config);
    
    MY_LOGI(TAG, "UART set pins, mode and install driver.");
    // Set UART1 pins(TX: IO17, RX: I016, RTS: IO32, CTS: IO--)
    uart_set_pin(RS485_UART_PORT, RS485_UART_TXD, RS485_UART_RXD, UART_2_RTS, UART_2_CTS);

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    //uart_driver_install(RS485_UART_PORT, RS485_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_driver_install(RS485_UART_PORT, RS485_RX_BUF_SIZE * 2, 0, 20, &RS485_QUEUE, 0);

#if CONFIG_BOARD_OLIMEX_ESP32_GATEWAY
    // Set RS485 half duplex mode
    uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
#endif
    
    //Create the semaphore for serial port.
    //rs485Semaphore = xSemaphoreCreateBinary();

    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "rs485_events", 2048, NULL, 12, NULL);

    //Assume serial port is ready
   //xSemaphoreGive(rs485Semaphore);
}

/* BICOM RS485 */
#ifdef BICOM_RS485_ENABLED
int get_bicom_485_state(int device)
{
  int ret;
  int state = -1;
  uint8_t modbus_data[10];

  if(settings.rs485[device].type != RS485_BISTABILE_SWITCH)
    return -1; //disabled state

  //if(detected_rs485_model_type[device] == NO_DEVICE) //if not detected
    //return -1;

  ret = read_modbus_registers(RS485_UART_PORT, settings.rs485[device].address, 30101, 1, modbus_data);
  if(ret)
    return ret;

  state = modbus_data[1];
  //MY_LOGI(TAG, "RS485 bicom %d state:%d", device, state);

  return state;
}

int get_bicom_485_address_state(int address)
{
  int ret;
  int state = -1;
  uint8_t modbus_data[10];

  ret = read_modbus_registers(RS485_UART_PORT, address, 30101, 1, modbus_data);
  if(ret)
    return ret;

  state = modbus_data[1];
  //MY_LOGI(TAG, "RS485 bicom %d state:%d", device, state);

  return state;
}

int set_bicom_485_device_state(int device, int state)
{
  int ret;

  if(settings.rs485[device].type != RS485_BISTABILE_SWITCH || state < 0 || state > 1)
    return 0;

  //if(detected_rs485_model_type[device] == NO_DEVICE) //if not detected
    //return -1;

  //no checking for state, because new bicoms responses to every state
  //if(state == get_bicom_485_state(device))
    //return 0; //if new state is current state, do nothing

  ret = modbus_write(RS485_UART_PORT, settings.rs485[device].address, 40141, state, 1000, NULL);

  return ret;
}

int set_bicom_485_address_state(uint8_t address, int state)
{
  int ret;

  ret = modbus_write(RS485_UART_PORT, address, 40141, state, 1000, NULL);

  return ret;
}

void bicom_485_toggle(int device)
{
	if(get_bicom_485_state(device) == 0)
		set_bicom_485_device_state(device, 1);
	else
		set_bicom_485_device_state(device, 0);
}

#if 0
static void bicom_rs485_task()
{
	EventBits_t uxBits;

	while(1)
	{
		uxBits = xEventGroupWaitBits(ethernet_event_group, BICOM_TASK_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & BICOM_TASK_STOP ) == BICOM_TASK_STOP )
		{
			MY_LOGI(TAG, "STOP RS485 BICOM POLLING!");
			vTaskDelete( NULL );
		}

		status.bicom_485_1_state = get_bicom_485_state(0);
		vTaskDelay(500 / portTICK_PERIOD_MS);
		status.bicom_485_2_state = get_bicom_485_state(1);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}
#endif

int checkRtuPacketComplete(uint8_t *buf, int len)
{
	int packet_len = 0;

	//printf("len:%d fc:%d\n", len, buf[1]);

	if(len < 5)
		return 0;

	//get length of modbus packet
	if(buf[1] == 3 || buf[1] == 4 || buf[1] == 6) //valid function codes
	{
		packet_len = 8; //packet_len = buf[2] + 5; //3bytes of header + 2 CRC
	}
	else if(buf[1] == 16) //write functions has fixed response length
	{
		packet_len = 9 + buf[6]; //buf[6] is number of bytes to write
	}
	else
		return 0;// not valid function code

	//printf("len:%d modbus:%d\n", len, packet_len);
//todo: check CRC
	if(packet_len == len)
		return 1;
	else
		return 0;
}
/*
 * On default settings modbus slave task and debug console tx runs at the same time
 * When 1st modbus packet arrives, debug console turns off
 */
#ifndef CONFIG_BOARD_IE14MW //not on IE
static void rs485_modbus_slave_task()
{
	EventBits_t uxBits;
	uint8_t modbus_buf[256];
	int send_len;
	int length = 0;

	MY_LOGI(TAG, "rs485_modbus_slave_task STARTED");

	//wait for uart data event
	while(1)
	{
#if 0
		uxBits = xEventGroupWaitBits(ethernet_event_group, MODBUS_SLAVE_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & MODBUS_SLAVE_STOP ) == MODBUS_SLAVE_STOP )
		{
			MY_LOGI(TAG, "STOP RS485 Modbus slave");
			vTaskDelete( NULL );
		}
#endif

		//waiting for uart data
		uxBits = xEventGroupWaitBits(uart_event_group, RS485_DATA_READY_BIT, true, true, 200 / portTICK_PERIOD_MS); //wait for another block

		if( ( uxBits & RS485_DATA_READY_BIT )  == RS485_DATA_READY_BIT )//new data?
		{
			int read_len = 0;
			if(debug_console == DEBUG_CONSOLE_485) //disable debug console if we receive something on rs485 bus
			{
				gpio_set_level(GPIO_NUM_2, 0); //disable 485 console tx
				debug_console = DEBUG_CONSOLE_INTERNAL; //turn off debug console till next reboot
			}

			uart_get_buffered_data_len(RS485_UART_PORT, (size_t*)&read_len);
			//printf("RS485 rec %d bytes\n\r", read_len);

			uart_read_bytes(RS485_UART_PORT, modbus_buf + length, read_len, PACKET_READ_TICS);
			length += read_len;

			if(checkRtuPacketComplete(modbus_buf, length))
			{
				//MY_LOGW(TAG,"TCP_SEMAPHORE_TAKE");
				BaseType_t sem = xSemaphoreTake(tcp_semaphore, TCP_SEMAPHORE_TIMEOUT); //Wait for serial port to be ready
				if(sem != pdTRUE )
				{
					MY_LOGE(TAG,"TCP_SEMAPHORE_TIMEOUT");
					//release semaphore even if we didnt took it
					//xSemaphoreGive(tcp_semaphore);-17.9.2020 bugfix?
					statistics.errors++;
					continue;
				}
				//printf("Packet complete.Len:%d\n\r", read_len);
				if(modbus_buf[0] == settings.sg_modbus_addr) //local modbus
				{
					send_len = modbus_slave((char *)modbus_buf, length, 0);
				}
				else if(modbus_buf[0] == settings.ir_counter_addr && settings.left_ir_enabled == 1)
				{
					//printf("IR path\n\r");
					//xSemaphoreTake(left_ir_Semaphore, portMAX_DELAY); //Wait for serial port to be ready
					if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
						status.left_ir_busy = 1;//active
					send_len = send_receive_modbus_serial(LEFT_IR_PORT_INDEX, (char *)modbus_buf, length, MODBUS_RTU);
					//xSemaphoreGive(left_ir_Semaphore);
				}
				else
				{
					MY_LOGE(TAG, "Wrong modbus address: %d", modbus_buf[0]);
					statistics.errors++;
					length = 0;
					//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
					xSemaphoreGive(tcp_semaphore);
					continue;
				}

				length = 0; //for next msg
				//printf("Receive %d bytes from device \n\r", send_len);
				if(send_len == 0)
				{
					MY_LOGW(TAG, "No data from device: %d", modbus_buf[0]);
					statistics.errors++;
					if(status.left_ir_busy == 1)//IR passthrough mode
						status.left_ir_busy = 0;//not active
					//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
					xSemaphoreGive(tcp_semaphore);
					continue;
				}

				esp_err_t ret = uart_wait_tx_done(RS485_UART_PORT, 2000 / portTICK_PERIOD_MS);
				if(ret != ESP_OK)
				{
					MY_LOGE(TAG, "ERROR uart_wait_tx_done, ret:%d", ret);
					statistics.errors++;
					if(status.left_ir_busy == 1)//IR passthrough mode
						status.left_ir_busy = 0;//not active
					//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
					xSemaphoreGive(tcp_semaphore);
					continue;
				}
				esp_log_level_set("*", ESP_LOG_NONE); //disable logging
				gpio_set_level(GPIO_NUM_2, 1); //enable 485 TX

				uart_write_bytes(RS485_UART_PORT, (char *)tcp_obuf, send_len);
				//release semaphore.
				//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
				xSemaphoreGive(tcp_semaphore);

				ret = uart_wait_tx_done(RS485_UART_PORT, 1000 / portTICK_PERIOD_MS);

				gpio_set_level(GPIO_NUM_2, 0); //disable 485 tx
				//esp_log_level_set("*", ESP_LOG_INFO);//restore logging
				set_log_level1(settings.log_disable1, ESP_LOG_WARN);
				set_log_level2(settings.log_disable2, ESP_LOG_WARN);
#if CONFIG_BOARD_SG    //disable logging for SG, because RS485 and log console share same uart
				esp_log_level_set(TAG, ESP_LOG_WARN); //485 logging limited to warning and errors
#endif

				if(ret != ESP_OK)
				{
					MY_LOGE(TAG, "ERROR uart_wait_tx_done");
					statistics.errors++;
					if(status.left_ir_busy == 1)//IR passthrough mode
						status.left_ir_busy = 0;//not active
					continue;
				}
			}
		}
		else if(length == 0) //no data yet, wait for 1st part of data
		{
			//vTaskDelay(10 / portTICK_PERIOD_MS);
			if(status.left_ir_busy == 1)//IR passthrough mode
				status.left_ir_busy = 0;//not active
			continue;
		}
		else //lenght in not 0, but no more data received
		{
			MY_LOGE(TAG, "ERROR modbus command timeout");
			statistics.errors++;
			if(status.left_ir_busy == 1)//IR passthrough mode
				status.left_ir_busy = 0;//not active
			length = 0;
		}
		//vTaskDelay(10 / portTICK_PERIOD_MS);
	}//while 1
}
#endif //#if CONFIG_BOARD_IE14MW

#define SEARCH_BY_SERIAL
#ifdef SEARCH_BY_SERIAL

detected_rs485_t rs485_detected_list[16];
int no_of_rs485_detected = 0;

void set_rs485_params(int baud_rate, uint8_t stop_bits)
{
	uart_config_t uart_config = {
				.baud_rate = baud_rate,
				.data_bits = UART_DATA_8_BITS,
				.parity = 0,
				//.stop_bits = stop_bits,
				.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
				.rx_flow_ctrl_thresh = 122,
		};

	ESP_LOGI(TAG,"set_rs485_params: %d bauds, stop bits:%d",baud_rate, stop_bits);

	if(stop_bits == 2)
		stop_bits = UART_STOP_BITS_2;

	uint32_t old_baud;
	uint32_t old_stop;
	uart_get_baudrate(RS485_UART_PORT, &old_baud);
	uart_get_stop_bits(RS485_UART_PORT, &old_stop);
	if(old_stop == stop_bits && old_baud < baud_rate*1.05 && old_baud > baud_rate*0.95)
		return;//no change in serial port params

	uart_config.stop_bits = stop_bits;
	ESP_LOGI(TAG,"OLD RS485       : %d bauds, stop bits:%d",old_baud, old_stop);
	ESP_LOGI(TAG,"Setting RS485 to: %d bauds, stop bits:%d",baud_rate, stop_bits);

	uart_flush_input(RS485_UART_PORT);
	uart_flush(RS485_UART_PORT);

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	ESP_ERROR_CHECK(uart_driver_delete(RS485_UART_PORT));

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));

	ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, RS485_RX_BUF_SIZE * 2, 0, 20, &RS485_QUEUE, 0));

	xTaskCreate(uart_event_task, "rs485_events", 2048, NULL, 12, NULL);
}

int read_modbus_by_serial(int USARTx, unsigned int startReg, unsigned int nrReg, uint8_t * serial_number)
{
	int ret;

	ret = read_modbus_registers(USARTx, 0, startReg, nrReg, serial_number);
	return ret;
}

int check_if_device_exists(char *serial)
{
  int i;

  for(i = 0; i < no_of_rs485_detected; i++)
  {
    if(strncmp(rs485_detected_list[i].serial_number, serial, 8) == 0)
      return 1;
  }
  return 0;
}

int add_device_to_list(int address, char *model, char* serial_number, int baud_rate, int parity, int stop_bits)
{

  int ret = check_if_device_exists(serial_number);
  if(ret)
  {
	  MY_LOGW(TAG, "Duplicate: %s", serial_number);
	  return -1;
  }

  MY_LOGI(TAG, "Adding to list :%d. Model: %s,Serial number: %s address: %d", no_of_rs485_detected, model, serial_number, address);

  rs485_detected_list[no_of_rs485_detected].address = address;
  rs485_detected_list[no_of_rs485_detected].baud_rate = baud_rate;
  rs485_detected_list[no_of_rs485_detected].parity = parity;
  rs485_detected_list[no_of_rs485_detected].stop_bits = stop_bits;
  strncpy(rs485_detected_list[no_of_rs485_detected].serial_number, serial_number, 8);
  strncpy(rs485_detected_list[no_of_rs485_detected].model, model, 16);
  no_of_rs485_detected++;

  return 0;
}

void print_detected_devices()
{
	int i;
	MY_LOGI(TAG, "Detected devices: %d", no_of_rs485_detected);

	for(i = 0; i < no_of_rs485_detected; i++)
	{
		MY_LOGI(TAG, "Device :%d %s %s %d", i+1,
		rs485_detected_list[i].model,
		rs485_detected_list[i].serial_number,
		rs485_detected_list[i].address);
	}
}

char *search_by_serial_number_JSON(int baud_rate, int parity, int stop_bits)
{
	int i;
	int ret;
	uint8_t modbus_data[25];
	int address;
	int digit;
	char *string = NULL;
	cJSON *devices = NULL;
	cJSON *device = NULL;

	for(digit = 7; digit > 1; digit--)
	{
		for(i = 0; i < 10; i++)//number from 0 to 9
		{
			char serial_number[9];
			char model[17];
			memset(modbus_data,'?',8); //start with all ?????????
			modbus_data[8] = 0; //terminate
			modbus_data[digit] = i + '0'; //modify one digit
			//printf("Modbus data:%s digit:%d\n\r", modbus_data, digit);
			ret = read_modbus_registers(RS485_UART_PORT, 0, 40202, 1, modbus_data);//address 0
			if (ret < 0)
				continue;
			address = modbus_data[1];
			if (address < 1 || address > 247)
				continue;

			MY_LOGI(TAG, "Detected address:%d baud rate: %d", address, baud_rate);
			memset(modbus_data,'?',8); //start with all ?????????
			modbus_data[8] = 0; //terminate
			modbus_data[digit] = i + '0'; //modify one digit
			ret = read_modbus_registers(RS485_UART_PORT, 0, 30001, 12, modbus_data);//read serial number with the same serial number mask
			if(ret == 0)
			{
				strncpy(model, (char*)modbus_data, 16);
				model[16] = 0;
				strncpy(serial_number, (char*)modbus_data+16, 8);
				serial_number[8] = 0;

				MY_LOGI(TAG, "Detected device at %d model:%s serial:%s", address, model, serial_number);
				if(no_of_rs485_detected > 15)
				{
					MY_LOGW(TAG, "Too many devices detected: %d", no_of_rs485_detected);
					break;
				}

				add_device_to_list(address, model, serial_number, baud_rate, parity, stop_bits);
			}
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}//i
	}//digit

	print_detected_devices();

	//create JSON
	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	devices = cJSON_CreateArray();
	if (devices == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "devices", devices);

	for(i= 0; i < no_of_rs485_detected; i++)
	{
		device = cJSON_CreateObject();
		if (device == NULL)
		{
			goto end;
		}
		cJSON_AddItemToArray(devices, device);

		cJSON_AddItemToObject(device, "model", cJSON_CreateString(rs485_detected_list[i].model));
		cJSON_AddItemToObject(device, "serial number", cJSON_CreateString(rs485_detected_list[i].serial_number));
		cJSON_AddItemToObject(device, "modbus address", cJSON_CreateNumber(rs485_detected_list[i].address));
		cJSON_AddItemToObject(device, "baud_rate", cJSON_CreateNumber(rs485_detected_list[i].baud_rate));
		cJSON_AddItemToObject(device, "stop_bits", cJSON_CreateNumber(rs485_detected_list[i].stop_bits));
	}

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	MY_LOGI(TAG, "%s", string);
	return string;	//return no_of_rs485_detected;
}

#endif //search by serial


#define RS485_TASK_STACK_SIZE 4096
#define RS485_TASK_PRIO 12
#endif //#ifdef BICOM_RS485_ENABLED

void start_rs485_master_task()
{
    rs485_init();

    //right_ir_init();
#ifdef BICOM_RS485_ENABLED
    //xTaskCreate(bicom_rs485_task, "485_bicom_task", RS485_TASK_STACK_SIZE, NULL, RS485_TASK_PRIO, NULL);
#endif
}

#if CONFIG_BOARD_SG
void start_rs485_slave_task()
{
	rs485_init();
	xTaskCreate(rs485_modbus_slave_task, "modbus_sl_task", RS485_TASK_STACK_SIZE, NULL, RS485_TASK_PRIO - 1, NULL);
}
#endif

#endif // ENABLE_RS485_PORT
