//********************************
//********************************
//********** TCP SERVER **********
//********************************
//********************************

#include "main.h"

#ifdef MODBUS_TCP_SERVER
//todo: naredi setting za debug
//#define DEBUG_TCP_SERVER

void tcp_serve(void *arg);
unsigned short CRC16(unsigned char *puchMsg, int usDataLen);

static const char * TAG = "TCP Server";
u8_t tcp_obuf[RS485_RX_BUF_SIZE];
int connected_clients = 0;

#if defined(ENABLE_LEFT_IR_PORT) && defined(ENABLE_RIGHT_IR_PORT)
static uart_params_t uart_param[3] = {
		{RS485_UART_PORT, RS485_DATA_READY_BIT, RS485_RX_BUF_SIZE},
		{RIGHT_IR_UART_PORT, RIGHT_IR_DATA_READY_BIT, RIGHT_IR_RX_BUF_SIZE},
		{LEFT_IR_UART_PORT, LEFT_IR_DATA_READY_BIT, LEFT_IR_RX_BUF_SIZE}
};
#else
static uart_params_t uart_param[3] = {
		{0, 0, 0},
		{0, 0, 0},
		{RS485_UART_PORT, RS485_DATA_READY_BIT, RS485_RX_BUF_SIZE}
};
#endif

//The semaphore for tcp buffer.
//xQueueHandle tcp_semaphore;
SemaphoreHandle_t tcp_semaphore = NULL;

void tcp_server_task(void *pvParam)
{
	int socket_id;
	int client_socket;

	MY_LOGI(TAG,"tcp_server task started");
	struct sockaddr_in tcpServerAddr;
	tcpServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpServerAddr.sin_family = AF_INET;
	tcpServerAddr.sin_port = htons( TCP_SERVER_PORT );
	static struct sockaddr_in remote_addr;
	static unsigned int socklen;
	socklen = sizeof(remote_addr);

	//----- WAIT FOR IP ADDRESS -----
	MY_LOGI(TAG, "waiting for SYSTEM_EVENT_ETH_GOT_IP ...");
	xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, portMAX_DELAY);
    MY_LOGI(TAG, "SYSTEM_EVENT_ETH_GOT_IP !!!!!!!!!");

#if CONFIG_BOARD_SG    //disable logging for SG, because RS485 and log console share same uart
#ifndef DEBUG_TCP_SERVER
#ifdef ENABLE_RS485_PORT
    //if(settings.rs485[0].type != 0 || settings.rs485[1].type != 0)
    if( countActiveRS485Devices() > 0)
    {
    	MY_LOGW(TAG, "LOGGING SET TO WARNING");
    	esp_log_level_set(TAG, ESP_LOG_WARN); //logging limited to warning and errors
    }
#endif //ENABLE_RS485_PORT
#endif
#endif

	while(1)
	{
		//----- ALLOCATE SOCKET -----
		socket_id = socket(AF_INET, SOCK_STREAM, 0);
		if(socket_id < 0)
		{
			//Couldn't allocate socket
			MY_LOGE(TAG, "... Failed to allocate socket.\n");
			statistics.errors++;
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		MY_LOGI(TAG, "... allocated socket\n");

		//----- BIND -----
		if(bind(socket_id, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0)
		{
			MY_LOGE(TAG, "... socket bind failed errno=%d \n", errno);
			statistics.errors++;
			shutdown(socket_id, SHUT_RDWR);
			close(socket_id);
			vTaskDelay(4000 / portTICK_PERIOD_MS);
			continue;
		}
		MY_LOGI(TAG, "... socket bind done \n");

		//----- LISTEN -----
		if(listen (socket_id, 2) != 0)
		{
			MY_LOGE(TAG, "... socket listen failed errno=%d \n", errno);
			statistics.errors++;
			shutdown(socket_id, SHUT_RDWR);
			close(socket_id);
			vTaskDelay(4000 / portTICK_PERIOD_MS);
			continue;
		}

		while (1)
		{
			//Once the socket has been created it doesn't matter if the network connection is lost, the Ethernet cable is unplugged and then re-plugged in,
			//this while loop will still continue to work the next time a client connects

			//----- WAIT FOR A CLIENT CONNECTION -----
			MY_LOGI(TAG,"Waiting for new client connection: %d...", connected_clients);
			client_socket = accept(socket_id,(struct sockaddr *)&remote_addr, &socklen);		//<<<< WE STALL HERE WAITING FOR CONNECTION
			if(client_socket < 0)
			{
				MY_LOGE(TAG,"Accept:%d", client_socket);
				statistics.errors++;
				perror ("accept");
				continue;
			}
			//connected_clients++;

			//----- A CLIENT HAS CONNECTED -----
#ifdef DEBUG_TCP_SERVER            
			MY_LOGI(TAG,"New client connection");
#endif
			//ret = sys_thread_new( "TCP MODBUS", tcp_serve, ( void * ) &client_socket, 512, TCP_SERVE_THREAD_PRIO);//DEFAULT_THREAD_STACKSIZE
			BaseType_t task_ret;
			task_ret = xTaskCreate(&tcp_serve, "tcp_serve", 3096, ( void * ) &client_socket, 6, NULL);
			if(task_ret != pdPASS)
			{
				MY_LOGE(TAG,"xTaskCreate:%d", task_ret);
				statistics.errors++;
				//continue;
			}
		}//while accept

		MY_LOGI(TAG, "TCP server will be opened again in 2 secs...");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		connected_clients = 0;

	}
	MY_LOGI(TAG, "TCP client task closed\n");
	vTaskDelete( NULL );
}

void tcp_serve(void *arg)
{
	int bytes_received;
	int idle_loop = 0;
	int tcp_send_len = 0;
	int *tmp = (int *)arg;
	int client_socket = *tmp;
	int modbus_address;
	int tcp_modbus;

	//Optionally set O_NONBLOCK
	//If O_NONBLOCK is set then recv() will return, otherwise it will stall until data is received or the connection is lost.
	fcntl(client_socket,F_SETFL,O_NONBLOCK);

	char *recv_buf = (char*) malloc(TCP_SERVER_BUFFER);
	if(recv_buf == NULL)
	{
		MY_LOGE(TAG, "%s CALLOC Failed", __FUNCTION__);
		statistics.errors++;
		//connected_clients--;
		vTaskDelete( NULL );
	}

	bzero(recv_buf, TCP_SERVER_BUFFER); //bzero(recv_buf, sizeof(recv_buf));
	connected_clients++;

	while (1)
	{
		EventBits_t uxBits;

		uxBits = xEventGroupWaitBits(ethernet_event_group, TCP_SERVER_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & TCP_SERVER_STOP ) == TCP_SERVER_STOP )
		{
			MY_LOGI(TAG, "STOP TCP SERVER!");
			//if(recv_buf)
				//free(recv_buf);
			//vTaskDelete( NULL );
			break;
		}

		bytes_received = recv(client_socket, recv_buf, TCP_SERVER_BUFFER-1, 0);		//<<<< WE STALL HERE WAITING FOR BYTES RECEIVED
		if (bytes_received == 0)
		{
			//----- CONNECTION LOST -----
			//There is no client any more - must have disconnected
			MY_LOGI(TAG,"Client connection lost c:%d",connected_clients);
			//if(recv_buf)
				//free(recv_buf);
			break;
		}
		else if (bytes_received < 0)
		{
			//----- NO DATA WAITING -----
			//We'll only get here if O_NONBLOCK was set
			if(idle_loop++ > 10*100)
			{
				MY_LOGW(TAG,"Client timeout");
				//if(recv_buf)
					//free(recv_buf);
				break; //after 1 minute of inactivity disconnect client
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);		//Release to RTOS scheduler
		}
		else
		{
			//----- TCP DATA RECEIVED -----
#ifdef DEBUG_TCP_SERVER                    
			MY_LOGI(TAG,"TCP Data received %d bytes, terminal_mode:%d", bytes_received, terminal_mode);
			//for(int i = 0; i < bytes_received; i++)
			//putchar(recv_buf[i]);
			//MY_LOGI(TAG,"Data receive complete");
#endif                    
			statistics.tcp_rx_packets++;
			idle_loop = 0;

			//----- SEND-RECEIVE TO/FROM SERIAL PORT -----
			//MY_LOGW(TAG,"TCP_SEMAPHORE_TAKE");
			if(xSemaphoreTake(tcp_semaphore, TCP_SEMAPHORE_TIMEOUT) != pdTRUE)  //Wait for serial port to be ready
			{
				MY_LOGE(TAG,"TCP_SEMAPHORE_TIMEOUT");
				//release semaphore even if we didn't took it
				//xSemaphoreGive(tcp_semaphore);17.9.2020 bugfix?
				statistics.errors++;
				break;
			}

			if(terminal_mode == 0)
			{
				tcp_modbus = check_tcp_header(recv_buf, bytes_received);
				modbus_address = recv_buf[tcp_modbus*6];
				//MY_LOGI(TAG, "Modbus address:%d", modbus_address);
			}
			else
				modbus_address = terminal_mode; //use terminal mode as modbus address

			if(modbus_address == settings.sg_modbus_addr) //local modbus
			{
				bytes_received = convert_to_rtu(recv_buf, bytes_received, &tcp_modbus);
				tcp_send_len = modbus_slave(recv_buf, bytes_received, tcp_modbus);
				if(0)
				{
					int i;
				    MY_LOGI(TAG,"TCP OUT size:%d tcp:%d",tcp_send_len, tcp_modbus);
				    for(i = 0; i < tcp_send_len; i++)
				      printf("%02x ", tcp_obuf[i]);
				    printf("\n\r");
				  }
			}
#ifdef ENABLE_LEFT_IR_PORT
			else if(modbus_address == settings.ir_counter_addr && settings.left_ir_enabled == 1)
			{
				if(settings.ir_ext_rel_mode == 2)//IR passthrough mode
					status.left_ir_busy = 1;//active
				tcp_send_len = send_receive_modbus_serial(LEFT_IR_PORT_INDEX, recv_buf, bytes_received, tcp_modbus);
			}
#endif
#ifdef ENABLE_RS485_PORT
			//else if(settings.rs485[0].type == RS485_PQ_METER || settings.rs485[0].type == RS485_ENERGY_METER
			//|| settings.rs485[1].type == RS485_PQ_METER || settings.rs485[1].type == RS485_ENERGY_METER)
			//else if( countActiveRS485Devices() > 0)
			else //everything else should go to RS485
			{
				//delay_us(5000); //todo: delay for modbus pause		//vTaskDelay(10 / portTICK_PERIOD_MS);
				tcp_send_len = send_receive_modbus_serial(RS485_PORT_INDEX, recv_buf, bytes_received, tcp_modbus);
			}
#endif
#if 0
			else
			{
				MY_LOGW(TAG, "No client for reply. Modbus address:%d", modbus_address);
				//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
				xSemaphoreGive(tcp_semaphore);
				break;
			}
#endif

			//----- TCP TRANSMIT -----
			if(tcp_send_len > 0 && tcp_send_len < RS485_RX_BUF_SIZE)
			{
				statistics.tcp_tx_packets++;
				if (write(client_socket , tcp_obuf , tcp_send_len) < 0)
				{
					MY_LOGE(TAG, "Transmit failed.Len:%d addr:%d", tcp_send_len, modbus_address);
					statistics.errors++;
					//close(socket_id);
					//vTaskDelay(1000 / portTICK_PERIOD_MS);
					//continue;
				}
#ifdef DEBUG_TCP_SERVER                        
				MY_LOGI(TAG, "Transmit complete.Len:%d", tcp_send_len);
#endif                                                
			}
			else
			{
				MY_LOGE(TAG, "Tcp transmit len. Len:%d addr:%d", tcp_send_len, modbus_address);
				statistics.errors++;
			}

			if(status.left_ir_busy == 1)//IR passthrough mode
				status.left_ir_busy = 0;//not active
			//release semaphore
			//MY_LOGW(TAG,"TCP_SEMAPHORE_GIVE");
			xSemaphoreGive(tcp_semaphore);

			//Clear the buffer for next time (not actually needed but may as well)
			bzero(recv_buf, TCP_SERVER_BUFFER);
		}
		//vTaskDelay(10 / portTICK_PERIOD_MS);		//Release to RTOS scheduler
	} //while (1)

	//We won't actually get here, the while loop never exits (unless its implementation gets changed!)

	//----- CLOSE THE SOCKET -----
	MY_LOGI(TAG, "Closing socket c:%d",connected_clients);
	shutdown(client_socket, SHUT_RDWR);
	close(client_socket);
	connected_clients--;
	if(recv_buf)
		free(recv_buf);
	vTaskDelete( NULL );
}

//check if it is TCP or rtu packet
int check_tcp_header(char *tcp_ibuf, int len)
{
    unsigned short size;
    
    if(len < 9)
        return 0; //rtu, because it is too short to be tcp
    
    size = (tcp_ibuf[4] << 8) + (tcp_ibuf[5] & 0xff);
    
    if(tcp_ibuf[2] == 0 && tcp_ibuf[3] == 0 && tcp_ibuf[4] == 0 && len == (size+TCP_MODBUS_HEADER_SIZE))  //TCP modbus: bytes 2 and 3 are 0, byte 4 is also 0 because size < 256, 25.1.2019: bugfix
    {
        return 1;	//tcp
    }
    else
        return 0; //rtu
}

int calculate_flash_data_transfer_size(uint8_t *buf, int len)
{
	int size = buf[6];

	if(size > 0 && size < 124)
	{
		int ret = 9 + 2 * (buf[6]);
		//printf("%d\r", ret);
		return ret;
	}

	if(size == 0)
	{
		int subfunction = buf[7];
		if(subfunction == 0)
			return 10;
		if(subfunction == 1)
			return 14;
		if(subfunction == 2 || subfunction == 3)
		{
			int ret = 16 + buf[12];
			return ret;
		}
	}

	if(size == 255)
	{
		int ret = 11 +  2* (buf[8]<<8 | buf[7]);
		return ret;
	}
	return 0;
}

int getModbusPacketSize(uint8_t *buf, int len)
{
	if(len < 5)
		return 0;

	//get length of modbus packet
	if(buf[1] == 3 || buf[1] == 4) //valid function codes
	{
		return buf[2] + 5; //3bytes of header + 2 CRC
	}
	else if(buf[1] == 6 || buf[1] == 16) //write functions has fixed response length
	{
		return 8;//7.7.2020: write response size is 8
	}
	else
		return 0;// not known packet
}

int checkPacketComplete(uint8_t *buf, int len)
{
	int packet_len = 0;

	if(len < 5)
		return 0;

	//get length of modbus packet
	if(buf[1] == 3 || buf[1] == 4) //valid function codes
	{
		packet_len = buf[2] + 5; //3bytes of header + 2 CRC
	}
	else if(buf[1] == 6 || buf[1] == 16) //write functions has fixed response length
	{
		packet_len = 8;//7.7.2020: write response size is 8
	}
	else
		return 0;// not valid function code

	if(packet_len != len)
		return 0; //wrong len
	else
		return 1; //packet complete
}

int send_receive_modbus_serial(int port, char *data, int len, int tcp_packet)
{
    int command_size = len;
    //int tcp_packet;
    u16_t crc = 0;
    uint8_t crc_buff[2];
    int length = 0;
    int packet_size = 0;
#ifndef SERIAL_CIRCULAR_BUFFER
    EventBits_t uxBits;
#endif
#if CONFIG_BOARD_SG
    int ret;
#endif
    
#if defined(CONFIG_BOARD_IE14MW) || defined(CONFIG_BOARD_OLIMEX_ESP32_GATEWAY)
    statistics.rs485_tx_packets++;
    port = RS485_UART_PORT;//only one port
#endif
    //printf("%s port:%d\n\r", __FUNCTION__,port);

//    tcp_packet = check_tcp_header(data, len);  //check if it is modbus tcp or modbus rtu over ethernet
    if(tcp_packet)
    {
        //convert Modbus TCP to Modbus RTU
        command_size = *(((u8_t *)data)+4)*256 + *(((u8_t *)data)+5);
        crc = CRC16( (unsigned char*) data+TCP_MODBUS_HEADER_SIZE, command_size);
        crc_buff[0] = (char)(crc >> 8);
        crc_buff[1] = (char)(crc & 0xff);
    }

    if(command_size > uart_param[port].buf_size)
    {
    	MY_LOGE(TAG, "***ERROR TCP IBUF len: %d\n\r", command_size);
        statistics.errors++;
        return 0;
    }   

#ifdef DEBUG_TCP_SERVER    
    ESP_LOG_BUFFER_HEXDUMP(TAG, data+tcp_packet*TCP_MODBUS_HEADER_SIZE, command_size, ESP_LOG_INFO);
#endif    
    
     /* S E N D to U A R T */
    xEventGroupClearBits(uart_event_group, uart_param[port].data_ready_bit); //clear uart data ready bit
    uart_flush_input(uart_param[port].port);

#if CONFIG_BOARD_SG
#ifdef ENABLE_RS485_PORT
    if(port == RS485_PORT_INDEX)
    {
    	ret = uart_wait_tx_done(uart_param[port].port, 2000 / portTICK_PERIOD_MS);
    	if(ret != ESP_OK)
    	{
    		MY_LOGE(TAG,"ERROR uart_wait_tx_done, ret:%d %s:%d", ret, __FUNCTION__, __LINE__);
    		return 0;
    	}
    	esp_log_level_set("*", ESP_LOG_NONE); //disable logging
    	gpio_set_level(GPIO_NUM_2, 1); //enable 485 TX
    	statistics.rs485_tx_packets++;
    }
#endif //#ifdef ENABLE_RS485_PORT
    //ir has crosstalk, so receive must be disabled during tx
    if(port == LEFT_IR_PORT_INDEX)
    {
    	statistics.left_ir_tx_packets++;
    	uart_disable_rx_intr(LEFT_IR_UART_PORT);
    }
    if(port == RIGHT_IR_PORT_INDEX)
    {
    	statistics.right_ir_tx_packets++;
    	uart_disable_rx_intr(RIGHT_IR_UART_PORT);
    }
#endif

    ret = uart_write_bytes(uart_param[port].port, (const char*)data+tcp_packet*TCP_MODBUS_HEADER_SIZE, command_size);
    if(ret < 0)
    {
    	MY_LOGE(TAG,"ERROR uart_write_bytes %s:%d", __FUNCTION__, __LINE__);
    	return 0;
    }
    
    if(tcp_packet)//send rtu crc
    {
    	ret = uart_write_bytes(uart_param[port].port, (const char*)crc_buff, 2);
    	if(ret < 0)
    	{
    		MY_LOGE(TAG,"ERROR uart_write_bytes %s:%d", __FUNCTION__, __LINE__);
    		return 0;
    	}
    }

#if CONFIG_BOARD_SG
#ifdef ENABLE_RS485_PORT
    if(port == RS485_PORT_INDEX)
    {
    	ret = uart_wait_tx_done(uart_param[port].port, 1000 / portTICK_PERIOD_MS);
    	if(ret != ESP_OK)
    	{
    		MY_LOGE(TAG,"ERROR uart_wait_tx_done %s:%d", __FUNCTION__, __LINE__);
    		return 0;
    	}
    	gpio_set_level(GPIO_NUM_2, 0); //disable 485 tx
    	//esp_log_level_set("*", ESP_LOG_INFO);//restore logging
    	uart_flush_input(RS485_PORT_INDEX);
    }
#endif //#ifdef ENABLE_RS485_PORT
    if(port == LEFT_IR_PORT_INDEX)
    {
    	ret = uart_wait_tx_done(uart_param[port].port, 1000 / portTICK_PERIOD_MS);
    	if(ret != ESP_OK)
    	{
    		MY_LOGE(TAG,"ERROR uart_wait_tx_done %s:%d", __FUNCTION__, __LINE__);
    		return 0;
    	}
    	uart_flush_input(LEFT_IR_UART_PORT);
    	uart_enable_rx_intr(LEFT_IR_UART_PORT); //enable rx irq
    }
    if(port == RIGHT_IR_PORT_INDEX)
    {
    	uart_flush_input(RIGHT_IR_UART_PORT);
    	uart_enable_rx_intr(RIGHT_IR_UART_PORT);
    }
#endif

    /* W A I T for R E S P O N S E */
#ifndef SERIAL_CIRCULAR_BUFFER
    xEventGroupClearBits(uart_event_group, uart_param[port].data_ready_bit); //clear uart data ready bit
    memset(tcp_obuf, 0, sizeof(tcp_obuf));
#endif

    //wait for uart data event
#ifdef SERIAL_CIRCULAR_BUFFER
    int timeout = 1000; //1000ms
    while(timeout)
    {
    	if(serBuffWrPnt[port] != serBuffRdPnt[port]) //new data
    	{
    		int read_len = 0;
    		read_len = serBuffWrPnt[port] - serBuffRdPnt[port];
    		if(read_len < 0)
    			read_len += SER_BUFF_SIZE; //overflow

    		//printf("UART%d %d bytes\n\r", port,read_len);

    		//uart_read_bytes(uart_param[port].port, tcp_obuf +tcp_packet*6 + length, read_len, PACKET_READ_TICS);
    		memcpy(tcp_obuf +tcp_packet*6 + length, &serBuff[0][port] + serBuffRdPnt[port], read_len);
    		serBuffRdPnt[port] = serBuffWrPnt[port];
    		length += read_len;

    		if(checkPacketComplete(tcp_obuf +tcp_packet*6, length))
    		{
    			//printf("Packet complete.Len:%d\n\r", length);
    			break; //packet is complete
    		}
    	}
    	else if(length > 0)//no more data
    		break;
    	else
    	{
    		vTaskDelay(10 / portTICK_PERIOD_MS);
    		timeout -= 10;
    	}
    }//while
    if(length == 0) //no data, so it is timeout
    {
    	MY_LOGE(TAG, "UART:%d Response timeout", port);
    	statistics.errors++;
    	//return 0;
    }
#else
    while(1)
    {
    	if(length == 0)//waiting for 1st packet is longer
    		uxBits = xEventGroupWaitBits(uart_event_group, uart_param[port].data_ready_bit, true, true, 1000 / portTICK_PERIOD_MS); //clear ready bit after call
    	else //waiting for next packet, pause between packets is shorter
    		uxBits = xEventGroupWaitBits(uart_event_group, uart_param[port].data_ready_bit, true, true, 200 / portTICK_PERIOD_MS); //wait for another block

    	if( ( uxBits & uart_param[port].data_ready_bit ) == uart_param[port].data_ready_bit )//new data?
    	{
    		int read_len = 0;
    		uart_get_buffered_data_len(uart_param[port].port, (size_t*)&read_len);
#ifdef DEBUG_TCP_SERVER
    		MY_LOGD(TAG,"UART%d %d bytes", port,read_len);
#endif
    		if(read_len == 0)
    		{
    			MY_LOGE(TAG, "UART:%d data len:%d", port, read_len);
    			printf("UART:%d ERR read len:%d", port, read_len);
    			statistics.errors++;
    			continue;//return 0;
    		}

    		uart_read_bytes(uart_param[port].port, tcp_obuf +tcp_packet*6 + length, read_len, PACKET_READ_TICS);
    		if(length == 0 && terminal_mode == 0)//first packet: get packet size for known packets
    		{
    			if(tcp_obuf[1 + tcp_packet*6] == 'D') //'D' flash data transfer function code
    				packet_size = calculate_flash_data_transfer_size(tcp_obuf +tcp_packet*6, read_len);

    			else if(tcp_obuf[1 + tcp_packet*6] == 'K') //'K' remote display
    				packet_size = 1024; //remote display size
    			else //modbus
    				packet_size = getModbusPacketSize(tcp_obuf +tcp_packet*6, read_len);
    		}

    		length += read_len; //add read length

    		if(terminal_mode && length > uart_param[port].buf_size - 128)
    		{
    			break;//prevent overflow
    		}
    		else if(packet_size > 0 && packet_size == length)
    		{
    			break; //packet complete
    		}
    	}
    	else if(length == 0) //no data, so it is timeout
    	{
    		MY_LOGE(TAG, "UART:%d Response timeout", port);
    		printf("UART:%d Response timeout addr:%d", port, tcp_obuf[0+tcp_packet*6]);
    		statistics.errors++;
    		break;//return 0;
    	}
    	else //no more data
    	{
#ifdef DEBUG_TCP_SERVER
    		printf("UART%d no more data. length:%d\n\r", port, length);
    		for(int i = 0; i < length; i++)
    			printf("%02x", tcp_obuf[tcp_packet*6 + i]);
    		printf("\n");
#endif
    		break;
    	}
    }//while 1
#endif //circular buffer

#if CONFIG_BOARD_SG //this solution eliminates uart frame errors

    if(port == LEFT_IR_PORT_INDEX)
    {
    	if(length > 0)
    		statistics.left_ir_rx_packets++;
    	if(settings.ir_ext_rel_mode != 2)//IR passthrough mode
    		uart_disable_rx_intr(LEFT_IR_UART_PORT); //disable rx irq because there can be IR crosstalk
    }
    if(port == RIGHT_IR_PORT_INDEX)
    {
    	if(length > 0)
    		statistics.right_ir_rx_packets++;
    	uart_disable_rx_intr(RIGHT_IR_UART_PORT); //disable rx irq because there can be IR crosstalk
    }
    if(port == RS485_PORT_INDEX)
    {
    	if(length > 0)
    		statistics.rs485_rx_packets++;
#ifndef DEBUG_TCP_SERVER
#ifdef ENABLE_RS485_PORT
    	//if(settings.rs485[0].type != 0 || settings.rs485[1].type != 0)
    	if( countActiveRS485Devices() > 0)
    	{
    		//esp_log_level_set("*", ESP_LOG_INFO);//restore global logging
    		set_log_level1(settings.log_disable1, ESP_LOG_WARN);
    		set_log_level2(settings.log_disable2, ESP_LOG_WARN);
    		esp_log_level_set(TAG, ESP_LOG_WARN); //this file logging limited to warning and errors
    	}
#endif //ENABLE_RS485_PORT
#else
    	esp_log_level_set("*", ESP_LOG_INFO);//restore logging
#endif //DEBUG_TCP_SERVER
    }
#endif //CONFIG_BOARD_SG

//#ifdef DEBUG_TCP_SERVER
#if 0
    MY_LOGI(TAG,"RS485 Received %d bytes", length);
    ESP_LOG_BUFFER_HEXDUMP(TAG, tcp_obuf, length, ESP_LOG_INFO);
#endif

    //delay 30us to generate pause on bus
    //delay_us(30);

    if(length == 0)
    	return 0;//no data, timeout

    //terminal mode detection
    if(length > 5)
    {
    	if(terminal_mode == 0)
    	{
    		ret = detect_terminal_mode(tcp_obuf+tcp_packet*6); //detect terminal mode
    		if(ret == 1)
    		{
    			terminal_mode = tcp_obuf[tcp_packet*6]; //use terminal mode as modbus address
    			MY_LOGI(TAG,"Terminal mode addr: %d", terminal_mode);
    		}
    	}
    }
    //terminal mode detection end

    //check received crc
    if(terminal_mode == 0)
    {
    	crc = CRC16( (unsigned char*) tcp_obuf+tcp_packet*6, length - 2);
    	if((tcp_obuf[tcp_packet*6 + length - 2] != (crc >> 8))	|| (tcp_obuf[tcp_packet*6 + length - 1] != (char)(crc & 0xff)))
    	{
    		printf("ERR CRC calc:%04x :%02x%02x tcp_packet:%d len:%d\n\r", crc, tcp_obuf[tcp_packet*6 + length - 2], tcp_obuf[tcp_packet*6 + length - 1], tcp_packet, length);
    		return 0;
    	}
    }
    //printf("CRC calc:%04x :%02x%02x\n\r", crc, tcp_obuf[tcp_packet*6 + length - 2], tcp_obuf[tcp_packet*6 + length - 1]);
    
    if(tcp_packet)
    {
        memcpy(tcp_obuf, data, 4);   //tcp header
        tcp_obuf[4] = (char)((length - 2) >> 8); //tcp size
        tcp_obuf[5] = (char)((length - 2) & 0xff); //tcp size        
        length += 4;//27.3.2018 size is +4 not +6 !!!!!!
    }

    if(terminal_mode)//detect END
    {
    	ret = detect_terminal_mode_end(tcp_obuf+tcp_packet*6);//detection of terminal mode end is made here!!!!!!!
    	if(ret == 0)
    	{
    		terminal_mode = 0;
    		//length = 9;   //if end detected, eliminate trailing packets (push)
    	}
    }

#ifdef DEBUG_TCP_SERVER
    MY_LOGI(TAG,"TCP reply %d bytes:", length);
    ESP_LOG_BUFFER_HEXDUMP(TAG, tcp_obuf, length, ESP_LOG_INFO);
#endif      
    
    return length;          
}
#endif //MODBUS_TCP_SERVER
