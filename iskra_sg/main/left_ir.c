/* Uart Events Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef ENABLE_LEFT_IR_PORT

static const char *TAG = "LEFT_IR";
static QueueHandle_t LEFT_IR_QUEUE;

#ifdef SERIAL_CIRCULAR_BUFFER
#define SER_BUFF_SIZE (2048)
uint8_t serBuff[SER_BUFF_SIZE][3];
int serBuffWrPnt[3], serBuffRdPnt[3];
#endif

//The semaphore indicating if serial port is ready.
//xQueueHandle left_ir_Semaphore;

int send_receive_bicom(char *command, int command_size, uint8_t *uart_right_ir_msg);

static void left_ir_uart_event_task(void *pvParameters)
{
    uart_event_t event;
    //size_t buffered_size;
    //uint8_t* dtmp = (uint8_t*) malloc(100);
    for(;;) {
        //Waiting for UART2 event.
        if(xQueueReceive(LEFT_IR_QUEUE, (void * )&event, (portTickType)portMAX_DELAY)) {
            //bzero(dtmp, RD_BUF_SIZE);
            //MY_LOGI(TAG, "uart[%d] event:", LEFT_IR_UART_PORT);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    xEventGroupSetBits(uart_event_group, LEFT_IR_DATA_READY_BIT);
#ifdef SERIAL_CIRCULAR_BUFFER
                    uart_read_bytes(LEFT_IR_UART_PORT, &serBuff[0][LEFT_IR_UART_PORT]+serBuffWrPnt[LEFT_IR_UART_PORT], event.size, portMAX_DELAY);
                    serBuffWrPnt[LEFT_IR_UART_PORT] += event.size;
                    serBuffWrPnt[LEFT_IR_UART_PORT] &= (SER_BUFF_SIZE - 1); //circular buffer
                    //MY_LOGI(TAG, "[UART DATA] size: %d WR:%d RD:%d", event.size, serBuffWrPnt[LEFT_IR_UART_PORT], serBuffRdPnt[LEFT_IR_UART_PORT]);
#endif
                    //MY_LOGI(TAG, "[UART DATA]: %d", event.size);
                    //uart_read_bytes(LEFT_IR_UART_PORT, dtmp, event.size, portMAX_DELAY);
                    //MY_LOGI(TAG, "[DATA EVT]:%c", dtmp[0]);
                    //uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    MY_LOGW(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(LEFT_IR_UART_PORT);
                    xQueueReset(LEFT_IR_QUEUE);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    MY_LOGW(TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(LEFT_IR_UART_PORT);
                    xQueueReset(LEFT_IR_QUEUE);
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
                    MY_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    //free(dtmp);
    //dtmp = NULL;
    vTaskDelete(NULL);
}

static void left_ir_init()
{
    uart_config_t uart_config = {
        .baud_rate = LEFT_IR_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    //rs485_event_group = xEventGroupCreate();

    // Set UART log level
    //esp_log_level_set(TAG, ESP_LOG_INFO);

    MY_LOGI(TAG, "Left IR Init");

    // Configure UART parameters
    uart_param_config(LEFT_IR_UART_PORT, &uart_config);

    MY_LOGI(TAG, "UART set pins, mode and install driver.");
    // Set UART1 pins(TX: IO17, RX: I016, RTS: IO32, CTS: IO--)
    uart_set_pin(LEFT_IR_UART_PORT, LEFT_IR_UART_TXD, LEFT_IR_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    //uart_driver_install(LEFT_IR_UART_PORT, LEFT_IR_RX_BUF_SIZE * 2, LEFT_IR_RX_BUF_SIZE * 2, 20, &LEFT_IR_QUEUE, 0);
    uart_driver_install(LEFT_IR_UART_PORT, LEFT_IR_RX_BUF_SIZE * 2, 0, 20, &LEFT_IR_QUEUE, 0);

    if(settings.ir_ext_rel_mode != 2)//IR passthrough mode
    	uart_disable_rx_intr(LEFT_IR_UART_PORT); //zurb todo: IR irq

    //Create the semaphore for serial port.
    //left_ir_Semaphore = xSemaphoreCreateBinary();

    //Create a task to handler UART event from ISR
    xTaskCreate(left_ir_uart_event_task, "left_ir_events", 2048, NULL, 12, NULL);

    //Assume serial port is ready
    //xSemaphoreGive(left_ir_Semaphore);
}

static void left_ir_passthrough_task()
{
	EventBits_t uxBits;
	uint8_t modbus_buf[256];

	MY_LOGI(TAG, "left_ir_passthrough_task STARTED");

	uart_enable_rx_intr(LEFT_IR_UART_PORT); //enable rx irq

	//wait for uart data event
	while(1)
	{
		uxBits = xEventGroupWaitBits(ethernet_event_group, BICOM_TASK_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & BICOM_TASK_STOP ) == BICOM_TASK_STOP )
		{
			MY_LOGI(TAG, "STOP IR PASSTHROUGH!");
			vTaskDelete( NULL );
		}

		//waiting for uart data
		uxBits = xEventGroupWaitBits(uart_event_group, LEFT_IR_DATA_READY_BIT, false, true, portMAX_DELAY); //wait left ir data

		if( (( uxBits & LEFT_IR_DATA_READY_BIT )  == LEFT_IR_DATA_READY_BIT) )//left IR data
		{
			if(status.left_ir_busy == 1)
				continue;

			//check again if event is still pending
			vTaskDelay(100 / portTICK_PERIOD_MS);
			uxBits = xEventGroupWaitBits(uart_event_group, LEFT_IR_DATA_READY_BIT, true, true, 100 / portTICK_PERIOD_MS);
			if( (( uxBits & LEFT_IR_DATA_READY_BIT )  == LEFT_IR_DATA_READY_BIT) )//IR passthrough data
			{
				if(status.left_ir_busy == 1)
					continue;

				int read_len = 0;
				xEventGroupClearBits(uart_event_group, LEFT_IR_DATA_READY_BIT); //clear uart data ready bit
				uart_get_buffered_data_len(LEFT_IR_UART_PORT, (size_t*)&read_len);

				MY_LOGI(TAG, "Left IR passthrough, %d bytes", read_len);

				if(read_len == 0)
				{
					MY_LOGE(TAG, "Left IR passthrough data len:%d", read_len);
					statistics.errors++;
					continue;//return 0;
				}

				uart_read_bytes(LEFT_IR_UART_PORT, modbus_buf, read_len, PACKET_READ_TICS);
#if 1
				if(read_len == 8)
				{
					uint8_t uart_right_ir_msg[20];
					int length = send_receive_bicom((char *)modbus_buf, read_len, uart_right_ir_msg);
					if(length < 0)
					{
						MY_LOGE(TAG, "ERROR send_receive_bicom");
						statistics.errors++;
						continue;
					}

					//check response
					if(uart_right_ir_msg[0] != 0x21 && uart_right_ir_msg[1] != 0x04 && uart_right_ir_msg[2] != 0x02 && uart_right_ir_msg[3] != 0x0)
					{
						MY_LOGE(TAG, "ERROR: Bicom response size: %d, data: ", length);
						MY_LOGE(TAG, "Bicom  addr:%d, fc:%d %d, %d", uart_right_ir_msg[0], uart_right_ir_msg[1], uart_right_ir_msg[2], uart_right_ir_msg[3]);
						statistics.errors++;
						//				for(i = 0; i < length; i++)
						//					printf("%x ", uart_right_ir_msg[i]);

						vTaskDelay(100 / portTICK_PERIOD_MS);
						continue;
					}

					uart_disable_rx_intr(LEFT_IR_UART_PORT);
					uart_write_bytes(LEFT_IR_UART_PORT, (char *)uart_right_ir_msg, length);
					uart_wait_tx_done(LEFT_IR_UART_PORT, 100 / portTICK_PERIOD_MS);
					uart_flush_input(LEFT_IR_UART_PORT);
					uart_enable_rx_intr(LEFT_IR_UART_PORT); //enable rx irq
				}
#endif
			}
		}
	}
}

void start_left_ir_task()
{
    left_ir_init();

    if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
    	xTaskCreate(left_ir_passthrough_task, "left_ir_pass", 3096, NULL, 12, NULL);
}
#endif //#ifdef ENABLE_LEFT_IR_PORT
