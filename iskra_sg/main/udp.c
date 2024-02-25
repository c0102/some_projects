/* BSD Socket API Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef UDP_SERVER

#define PORT 33333
#define UDP_QUERY_SIZE  204

#define UDP_FETCH_INFO	0x1A
#define UDP_CHANGE_IP	0x1C
#define UDP_RESET_ETH	0x1D

#define CONFIG_EXAMPLE_IPV4

static const char *TAG = "UDP";
static char info_ihub[200];
//static char hw_version[2] ="A";

extern const esp_app_desc_t *app_desc;

void setIhubUdpInfo()
{
	int n1, n2, n3, n4;

	memset(info_ihub, 0, 200);

	//add IP
	sscanf(status.ip_addr, "%d.%d.%d.%d", &n1, &n2, &n3, &n4);
	info_ihub[0] = n1;
	info_ihub[1] = n2;
	info_ihub[2] = n3;
	info_ihub[3] = n4;

	if(settings.connection_mode != ETHERNET_MODE)
		memcpy(&info_ihub[0] + 4, &settings.wifi_ssid, 12);
	memcpy(&info_ihub[0] + 16, &status.mac_addr, 6);
	memcpy(&info_ihub[0] + 22, &settings.tcp_port, 2);
	memcpy(&info_ihub[0] + 24, &factory_settings.model_type ,15);
	memcpy(&info_ihub[0] + 39, &factory_settings.serial_number ,8);
	info_ihub[48] = strtol(app_desc->version, NULL, 10); //sw version
	info_ihub[50] = factory_settings.hw_ver + 'A';
	memcpy(&info_ihub[0] + 51, &settings.description ,40);
	memcpy(&info_ihub[0] + 91, &settings.location ,40);
	info_ihub[132] = settings.sg_modbus_addr;
	info_ihub[134] = strtol(status.fs_ver, NULL, 10);
}

void udp_server_task(void *pvParameters)
{
    char rx_buffer[256];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    //----- WAIT FOR IP ADDRESS -----
    MY_LOGI(TAG, "waiting for SYSTEM_EVENT_ETH_GOT_IP ...");
    xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, portMAX_DELAY);
    MY_LOGI(TAG, "SYSTEM_EVENT_ETH_GOT_IP !!!!!!!!! \n");

    setIhubUdpInfo();

    while (1) {

#ifdef CONFIG_EXAMPLE_IPV4
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        struct sockaddr_in6 dest_addr;
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            MY_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        MY_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            MY_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        MY_LOGI(TAG, "Socket bound, port %d", PORT);

        while (1) {

            MY_LOGI(TAG, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                MY_LOGE(TAG, "recvfrom failed: errno %d", errno);
                statistics.errors++;
                break;
            }
            // Data received
            else {
            	// Get the sender's ip address as string
            	if (source_addr.sin6_family == PF_INET) {
            		inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
            	} else if (source_addr.sin6_family == PF_INET6) {
            		inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
            	}

            	rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
            	MY_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            	MY_LOGI(TAG, "%s", rx_buffer);

            	if(rx_buffer[0] == 0 && rx_buffer[1] == 0 && rx_buffer[2] == 0 && rx_buffer[3] == UDP_FETCH_INFO)//Query for info record
            	{
            		rx_buffer[0] = 0;
            		rx_buffer[1] = 0;
            		rx_buffer[2] = 0;
            		rx_buffer[3] = 0x1b; //Device info

            		setIhubUdpInfo();

            		memcpy(rx_buffer+4, info_ihub, 200);

            		//sendto(socket_fd,rx_buffer,UDP_QUERY_SIZE,0,(struct sockaddr *)&sa,addrlen);
            		MY_LOGI(TAG, "UDP INFO received");

            		int err = sendto(sock, rx_buffer, UDP_QUERY_SIZE, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            		if (err < 0) {
            			MY_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            			statistics.errors++;
            			break;
            		}
            	}
            	else if(rx_buffer[0] == 0 && rx_buffer[1] == 0 && rx_buffer[2] == 0 && rx_buffer[3] == UDP_RESET_ETH)//reset
            	{
            		MY_LOGI(TAG, "UDP Reset received");
            		reset_device();
            	}
            }
        }

        if (sock != -1) {
            MY_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
#endif //UDP_SERVER
