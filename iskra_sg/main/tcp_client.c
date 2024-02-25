/*
 * tcp_client.c
 *
 *  Created on: Nov 25, 2019
 *      Author: iskra
 */

#include "main.h"

static const char *TAG = "TCP_client";

void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    tcp_client_params_t *params = (tcp_client_params_t *)pvParameters;

    statistics.push++;
    //while (1) {

    char *push_buf = calloc(4096, 1);
    MY_LOGI(TAG, "Push ptr:%x",(int)push_buf);
    int ret = getEnergyMeasurementsXML(push_buf, status.modbus_device[params->device].uart_port,
    		status.modbus_device[params->device].address,	status.modbus_device[params->device].push_interval);
    if(ret > 0)
    {
    	//printf("Push:%s",push_buf);

#define CONFIG_EXAMPLE_IPV4
#ifdef CONFIG_EXAMPLE_IPV4
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(params->ip_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(params->port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        struct sockaddr_in6 dest_addr;
        inet6_aton(HOST_IP_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            MY_LOGE(TAG, "Unable to create socket: errno %d", errno);
            statistics.push_errors++;
            goto end; //break;
        }
        MY_LOGI(TAG, "Socket created, connecting to %s:%d datalen: %d ptr:%x", params->ip_addr, params->port, strlen(push_buf), (int)push_buf);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            MY_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            statistics.push_errors++;
            goto end; //break;
        }
        MY_LOGI(TAG, "Successfully connected");

        do{ //while (1) {
        	MY_LOGI(TAG, "Sending len:%d", strlen(push_buf));
        	//MY_LOGI(TAG, "Sending data:%s", params->data_ptr);
            int err = send(sock, push_buf, strlen(push_buf), 0);
            if (err < 0) {
                MY_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                statistics.push_errors++;
                break;
            }
            else
            	MY_LOGI(TAG, "Sent len:%d", strlen(push_buf));

            struct timeval tv;
            fd_set readfds;

            tv.tv_sec = settings.publish_server[params->link].push_resp_time;   //tv.tv_sec = 10;
            tv.tv_usec = 0;

            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            select(sock+1, &readfds, NULL, NULL, &tv);

            if (FD_ISSET(sock, &readfds))
            {
            	// n = read(sock, recvBuff, 200);  //read ack

            	int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            	// Error occurred during receiving
            	if (len < 0) {
            		MY_LOGE(TAG, "recv failed: errno %d", errno);
            		statistics.push_errors++;
            		break;
            	}
            	// Data received
            	else {
            		rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
            		MY_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            		MY_LOGI(TAG, "%s", rx_buffer);
            		statistics.push_ack++;
            	}

            	//vTaskDelay(2000 / portTICK_PERIOD_MS);
            }
            else
            	MY_LOGW(TAG, "No ACK");

        }while (0);
end:
        if (sock != -1) {
            MY_LOGI(TAG, "Shutting down socket...");
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
    //}//while 1
    }
    MY_LOGI(TAG, "Free ptr:%x", (int)push_buf);
    free(push_buf); //free push buffer
    vTaskDelete(NULL);
}
