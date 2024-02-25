/*
 * bicom.c
 *
 *  Created on: Sep 4, 2019
 *      Author: iskra
 */

#include "include/main.h"

#ifdef ENABLE_RIGHT_IR_PORT

static const char *TAG = "bicom";
static QueueHandle_t RIGHT_IR_QUEUE;

int send_receive_bicom(char *command, int command_size, uint8_t *uart_right_ir_msg);

static void right_ir_uart_event_task(void *pvParameters)
{
	uart_event_t event;
	//size_t buffered_size;
	//uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
	for(;;) {
		//Waiting for UART1 event.
		if(xQueueReceive(RIGHT_IR_QUEUE, (void * )&event, (portTickType)portMAX_DELAY)) {
			//bzero(dtmp, RD_BUF_SIZE);
			//MY_LOGI(TAG, "uart[%d] event:", RIGHT_IR_UART_PORT);
			switch(event.type) {
			//Event of UART receving data
			/*We'd better handler data event fast, there would be much more data events than
                        other types of events. If we take too much time on data event, the queue might
                        be full.*/
			case UART_DATA:
				xEventGroupSetBits(uart_event_group, RIGHT_IR_DATA_READY_BIT);
#ifdef SERIAL_CIRCULAR_BUFFER
				uart_read_bytes(RIGHT_IR_UART_PORT, &serBuff[0][RIGHT_IR_UART_PORT]+serBuffWrPnt[RIGHT_IR_UART_PORT], event.size, portMAX_DELAY);
				serBuffWrPnt[RIGHT_IR_UART_PORT] += event.size;
				serBuffWrPnt[RIGHT_IR_UART_PORT] &= (SER_BUFF_SIZE - 1); //circular buffer
#endif
				//MY_LOGI(TAG, "[UART DATA]: %d", event.size);
				//uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
				//MY_LOGI(TAG, "[DATA EVT]:");
				//uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
				break;
				//Event of HW FIFO overflow detected
			case UART_FIFO_OVF:
				MY_LOGW(TAG, "hw fifo overflow");
				// If fifo overflow happened, you should consider adding flow control for your application.
				// The ISR has already reset the rx FIFO,
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(RIGHT_IR_UART_PORT);
				xQueueReset(RIGHT_IR_QUEUE);
				break;
				//Event of UART ring buffer full
			case UART_BUFFER_FULL:
				MY_LOGW(TAG, "ring buffer full");
				// If buffer full happened, you should consider increasing your buffer size
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(RIGHT_IR_UART_PORT);
				xQueueReset(RIGHT_IR_QUEUE);
				break;
				//Event of UART RX break detected
			case UART_BREAK:
				//MY_LOGW(TAG, "uart rx break");
				break;
				//Event of UART parity check error
			case UART_PARITY_ERR:
				MY_LOGW(TAG, "uart parity error");
				break;
				//Event of UART frame error
			case UART_FRAME_ERR:
				MY_LOGW(TAG, "uart frame error");
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
				MY_LOGW(TAG, "right uart event type: %d", event.type);
				break;
			}
		}
	}
	//free(dtmp);
	//dtmp = NULL;
	vTaskDelete(NULL);
}

int get_ir_bicom_state()
{
	char command[]={0x21, 0x04, 0x0, 0xC5, 0x0, 0x01, 0x26, 0x97};
	uint8_t uart_right_ir_msg[20];
	int command_size = 8;
	int i;
	int state = -1;
	int retry = 3;

	if(settings.ir_ext_rel_mode == 0)
		return -1;

	while(retry--)
	{
		int length;
		length = send_receive_bicom(command, command_size, uart_right_ir_msg);
		if(length < 0)
		{
			MY_LOGE(TAG, "ERROR send_receive_bicom");
			statistics.errors++;
			return -1;
		}

		//check response
		if(uart_right_ir_msg[0] != 0x21 && uart_right_ir_msg[1] != 0x04 && uart_right_ir_msg[2] != 0x02 && uart_right_ir_msg[3] != 0x0)
		{
			MY_LOGE(TAG, "ERROR: Bicom response size: %d, data: ", length);
			MY_LOGE(TAG, "Bicom  addr:%d, fc:%d %d, %d", uart_right_ir_msg[0], uart_right_ir_msg[1], uart_right_ir_msg[2], uart_right_ir_msg[3]);
			statistics.errors++;
			for(i = 0; i < length; i++)
				printf("%x ", uart_right_ir_msg[i]);

			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}

		state = uart_right_ir_msg[4];
		if(state > 1)
		{
			printf("ERROR Bicom state: %d\n\r", state);
			//continue;
		}
		else
		{
			break; //read state OK
		}
	}  //retry

	status.bicom_state = state;
	return state;
}

int set_ir_bicom_state(int state)
{
  char command[]={0x21, 0x06, 0x0, 0x0F, 0x00, 0x01, 0x7F, 0x69}; //ON command
  char response[]={0x21, 0x2A, 0x00, 0x0F, 0x00, 0x01, 0xEE, 0xAF}; //ON response
  uint8_t uart_right_ir_msg[120];
  int command_size = 8;
  int length = 0;
  int i;

  if(state == 0)//off, modify command
  {
    //modify command
    command[5] = 0;
    command[6] = 0xBE;
    command[7] = 0xA9;
    //modify response
    response[5] = 0;
    response[6] = 0x2F;
    response[7] = 0x6F;
  }

  length = send_receive_bicom(command, command_size, uart_right_ir_msg);
  if(length < 0)
  {
	  MY_LOGE(TAG, "ERROR send_receive_bicom");
	  statistics.errors++;
	  return -1;
  }

#if 1
  MY_LOGI(TAG, "Bicom Received %d bytes\n\r", length);
  ESP_LOG_BUFFER_HEXDUMP(TAG, uart_right_ir_msg, length, ESP_LOG_INFO);
#endif

  //check response
  for(i = 0; i < command_size; i++)
  {
    if(response[i] != uart_right_ir_msg[i])
    {
    	MY_LOGE(TAG, "ERROR Set bicom state i=%d %x:%x\n\r", i, response[i], uart_right_ir_msg[i]);
    	statistics.errors++;
    	return -1;
    }
  }

  status.bicom_state = state;

  return 0;
}

void bicom_toggle()
{
	if(get_ir_bicom_state() == 0)
		set_ir_bicom_state(1);
	else
		set_ir_bicom_state(0);
}

void right_ir_init()
{
    uart_config_t uart_config = {
        .baud_rate = RIGHT_IR_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // Set UART log level
    //esp_log_level_set(TAG, ESP_LOG_INFO);

    MY_LOGI(TAG, "Right IR Init");

    // Configure UART parameters
    uart_param_config(RIGHT_IR_UART_PORT, &uart_config);

    MY_LOGI(TAG, "UART set pins, mode and install driver.");
    // Set UART1 pins(TX: IO17, RX: I016, RTS: IO32, CTS: IO--)
    uart_set_pin(RIGHT_IR_UART_PORT, RIGHT_IR_UART_TXD, RIGHT_IR_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    uart_driver_install(RIGHT_IR_UART_PORT, RIGHT_IR_RX_BUF_SIZE * 2, 0, 20, &RIGHT_IR_QUEUE, 0);

    uart_disable_rx_intr(RIGHT_IR_UART_PORT); //zurb todo: IR irq

    //Create a task to handler UART event from ISR
    xTaskCreate(right_ir_uart_event_task, "right_ir_events", 2048, NULL, 12, NULL);
}

void right_ir_task()
{
	right_ir_init();
}

int send_receive_bicom(char *command, int command_size, uint8_t *uart_right_ir_msg)
{
	EventBits_t uxBits;
	size_t length = 0;

	/* S E N D to U A R T */
	statistics.right_ir_tx_packets++;
	uart_wait_tx_done(RIGHT_IR_UART_PORT, 1000);
	uart_disable_rx_intr(RIGHT_IR_UART_PORT);
	uart_write_bytes(RIGHT_IR_UART_PORT, command, command_size);  //send to bicom
	uart_wait_tx_done(RIGHT_IR_UART_PORT, 1000);

	uart_flush_input(RIGHT_IR_UART_PORT);
	uart_enable_rx_intr(RIGHT_IR_UART_PORT);
	xEventGroupClearBits(uart_event_group, RIGHT_IR_DATA_READY_BIT); //clear uart data ready bit

	/* W A I T for R E S P O N S E */
	//wait for uart data event
	uxBits = xEventGroupWaitBits(uart_event_group, RIGHT_IR_DATA_READY_BIT, true, true, 1000 / portTICK_PERIOD_MS); //clear ready bit after call
	if( ( uxBits & RIGHT_IR_DATA_READY_BIT ) == RIGHT_IR_DATA_READY_BIT )
	{
		statistics.right_ir_rx_packets++;
		uart_get_buffered_data_len(RIGHT_IR_UART_PORT, &length);
		//MY_LOGI(TAG, "Right IR received len:%d", length);
		length = uart_read_bytes(RIGHT_IR_UART_PORT, uart_right_ir_msg, length, PACKET_READ_TICS);
		uart_disable_rx_intr(RIGHT_IR_UART_PORT);
		return length;
	}
	else
	{
		MY_LOGE(TAG, "Right IR Response timeout");
		statistics.errors++;
		uart_disable_rx_intr(RIGHT_IR_UART_PORT);
		return -1;
	}
}
#endif //#ifdef ENABLE_RIGHT_IR_PORT
