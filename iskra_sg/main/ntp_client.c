#include "main.h"

//https://github.com/G6EJD/ESP32-Time-Services-and-SETENV-variable/blob/master/README.md
#ifdef NTP_ENABLED
static const char * TAG = "NTP Client";
static void set_timezone(signed short offset, char *timezone);

static int initialize_sntp(void)
{
    MY_LOGI(TAG, "Initializing SNTP");

    if(settings.ntp_server1[0] == 0 && settings.ntp_server2[0] == 0 && settings.ntp_server3[0] == 0)//ntp server not set, exit
    {
    	MY_LOGE(TAG, "ntp server not set");
    	//strncpy(settings.ntp_server1, CONFIG_NTP_SERVER, 32);
    	return -1;
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, settings.ntp_server1);
    sntp_setservername(1, settings.ntp_server2);
    sntp_setservername(2, settings.ntp_server3);

    sntp_init();
    return 0;
}

void ntp_client_task(void *pvParam)
{
	char timezone[50] = {'\0'};
	MY_LOGI(TAG,"ntp_client task started");

	//----- WAIT FOR IP ADDRESS -----
	MY_LOGI(TAG, "waiting for SYSTEM_EVENT_ETH_GOT_IP ...");
	xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, portMAX_DELAY);
	MY_LOGI(TAG, "SYSTEM_EVENT_ETH_GOT_IP !!!!!!!!!");

	int ret = initialize_sntp();
	if(ret < 0)
		vTaskDelete( NULL );

	set_timezone(settings.timezone, timezone);
	//setenv("TZ", "CEST-1CET,M3.5.0/2,M10.5.0/3", 1);
	setenv("TZ", timezone, 1);
	tzset();

	vTaskDelete( NULL );
#if 0
	while(1)
	{
		time_t now;
		struct tm timeinfo;

		time(&now);
		status.timestamp = now;
		localtime_r(&now, &timeinfo);
		strftime(status.local_time, sizeof(status.local_time),"%d.%m.%Y %H:%M:%S", &timeinfo);
		//MY_LOGI(TAG, "The current date/time is: %s", status.local_time);

		if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED){
			MY_LOGI(TAG, "Time is now set. Server:%s", settings.ntp_server1);
			//vTaskDelay(5000 / portTICK_PERIOD_MS);
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);

		//#ifdef DISPLAY_STACK_FREE

    UBaseType_t uxHighWaterMark;

    /* Inspect our own high water mark on entering the task. */
    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

    MY_LOGI(TAG, "FREERTOS STACK: %d, Heap free size: %d\n\r",
            uxHighWaterMark, xPortGetFreeHeapSize());
            
    MY_LOGI(TAG, "ESP Heap: %d, ESP min Heap: %d", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
  }//while 1
#endif
}

typedef struct sg_timezone
{
	char *zonename;
	char *timezone;
    int offset;
}timezone_t;

static void set_timezone(signed short offset, char* timezone)
{
	//char zonename[50] = {'\0'};
#define MAX_TZ 31
	timezone_t TZ[MAX_TZ] = {
	        { "Europe/London", "GMT0BST,M3.5.0/1,M10.5.0", 0 },
	        { "Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3", 60 },
	        { "Europe/Istanbul", "EET-2EEST,M3.5.0/3,M10.5.0/4", 120 },
	        { "Asia/Kuwait", "AST-3", 180 },
	        { "Asia/Dubai", "GST-4", 240 },
	        { "Asia/Kabul", "AFT-4:30", 270 },
	        { "Asia/Karachi", "PKT-5", 300 },
	        { "Asia/Kolkata", "IST-5:30", 330 },
	        { "Asia/Kathmandu", "NPT-5:45", 345 },
	        { "Asia/Dhaka", "BDT-6", 360 },
	        { "Asia/Rangoon", "MMT-6:30", 390 },
	        { "Asia/Bangkok", "ICT-7", 420 },
	        { "Asia/Hong Kong", "HKT-8", 480 },
	        { "Asia/Tokyo", "JST-9", 540 },
	        { "Australia/Adelaide", "CST-9:30CST,M10.1.0,M4.1.0/3", 570 },
	        { "Australia/Sydney", "EST-10EST,M10.1.0,M4.1.0/3", 600 },
	        { "Asia/Sakhalin", "SAKT-11", 660 },
	        { "Pacific/Fiji", "FJT-12", 720 },
	        { "Pacific/Apia", "WST-13", -660 },
	        { "Pacific/Honolulu", "HST10", -600 },
	        { "America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0", -540 },
	        { "Pacific/Pitcairn", "PST8", -480 },
	        { "America/Phoenix", "MST7", -420 },
	        { "America/Mexico City", "CST6CDT,M4.1.0,M10.5.0", -360 },
	        { "America/New York", "EST5EDT,M3.2.0,M11.1.0", -300 },
	        { "America/Caracas", "VET4:30", -270 },
	        { "America/La Paz", "BOT4", -240 },
	        { "America/St Johns", "NST3:30NDT,M3.2.0,M11.1.0", -210 },
	        { "America/Argentina/Buenos Aires", "ART3", -180 },
	        { "Atlantic/South Georgia", "GST2", -120 },
	        { "Atlantic/Azores", "AZOT1AZOST,M3.5.0/0,M10.5.0/1", -60 }
	    };

	int i;
	int found = 0;

	for(i = 0; i < MAX_TZ; i++)
	{
		if(TZ[i].offset == offset)
		{
			//strcpy(zonename, TZ[i].zonename);
			strcpy(timezone, TZ[i].timezone);
			MY_LOGI(TAG, "INFO(MODBUS) Timezone set to: %s %s, offset: %d", TZ[i].zonename, timezone, offset);
			found = 1;
			break;
		}
	}

	if(found == 0)
	{
		MY_LOGE(TAG, "ERROR(MODBUS) Timezone offset %d (0x%04x) not found", offset, offset);
		return;
	}

}

#endif //NTP_ENABLED
