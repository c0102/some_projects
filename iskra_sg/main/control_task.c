
#include "main.h"

#define ARRAY_SIZE_OFFSET   (5)   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE
#define STATS_TICKS         pdMS_TO_TICKS(10000) //10 seconds tasks statistics
#define TASK_STATS_SIZE (30)

static const char *TAG = "control_task";

//CONFIG_USE_TRACE_FACILITY must be defined as y in menuconfig/Component config/FreeRTOS for uxTaskGetSystemState() to be available.
//CONFIG_USE_STATS_FORMATTING_FUNCTIONS
static esp_err_t print_real_time_stats(TickType_t xTicksToWait, int *cpu_usage_percentage, char *write_buf)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    int idle_time = 0;
    int stack_free = 0;
    esp_err_t ret;
    char tmp_buf[TASK_STATS_SIZE];
    char taskname[configMAX_TASK_NAME_LEN + 1];
    
    // Make sure the write buffer does not contain a string.
    *write_buf = 0x00; 

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        MY_LOGE(TAG,"ERROR start time = end time:%d", start_run_time);
        statistics.errors++;
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    ESP_LOGI(TAG,"| Task             | Run Time | Stack | Percentage");
    ESP_LOGI(TAG,"|------------------|----------|-------|-----------");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //get stack usage
                stack_free = uxTaskGetStackHighWaterMark(start_array[i].xHandle);
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            strncpy(taskname, start_array[i].pcTaskName, configMAX_TASK_NAME_LEN);
            ESP_LOGI(TAG,"| %-16s | %8d | %5d | %2d%%", taskname, task_elapsed_time, stack_free, percentage_time );
            sprintf(tmp_buf, "%-16s:%2d%% free:%d\n", taskname, percentage_time, stack_free); //fill buffer for mqtt
            strcat(write_buf, tmp_buf);
            if(strncmp(start_array[i].pcTaskName, "IDLE", 4) == 0)
                idle_time += percentage_time;
        }
    }

    //Print unmatched tasks
#if 0 //dont show deleted tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
        	strncpy(taskname, start_array[i].pcTaskName, configMAX_TASK_NAME_LEN);
            ESP_LOGI(TAG,"| %-16s | Deleted", taskname);
        }
    }
#endif

    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
        	strncpy(taskname, end_array[i].pcTaskName, configMAX_TASK_NAME_LEN);
            ESP_LOGI(TAG,"| %-16s | Created", end_array[i].pcTaskName);
        }
    }
    
    ESP_LOGI(TAG,"|------------------|----------|-------|-----------");
    ESP_LOGI(TAG,"| %-16s | %8d | %5d | %2d%%", "CPU usage", 0, 0, 100 - idle_time);
    *cpu_usage_percentage = 100 - idle_time;    
    
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

static void control_task()
{
#define CPU_USAGE_SAMPLES (3) //3 values for 30 seconds average
    esp_err_t err;
    int cpu_usage[CPU_USAGE_SAMPLES];
    int cpu_usage_index = 0;
    char *tasks_stats = NULL;
    
    for(int i = 0; i < CPU_USAGE_SAMPLES; i++)
    	cpu_usage[i] = 0;

    //Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(100));
    
    while(1)
    {
        int tasks_size = TASK_STATS_SIZE * uxTaskGetNumberOfTasks() + 200;//prepare space for tasks and 200 bytes for heap and uptime
        MY_LOGD(TAG,"Allocating size:%d", tasks_size);
        tasks_stats = malloc(tasks_size);
        if (tasks_stats != NULL) {           
            err = print_real_time_stats(STATS_TICKS, &cpu_usage[cpu_usage_index], tasks_stats);
            if (err == ESP_OK) {
            	//MY_LOGI(TAG,"Real time stats obtained");
            	status.cpu_usage = 0;
            	for(int i = 0; i < CPU_USAGE_SAMPLES; i++)
            		status.cpu_usage += cpu_usage[i];
            	status.cpu_usage /= CPU_USAGE_SAMPLES;
            	if(cpu_usage_index++ > CPU_USAGE_SAMPLES)
            		cpu_usage_index = 0;
            } else {
                MY_LOGE(TAG,"Error getting real time stats. err:%d\n", err);
                statistics.errors++;
            }

#ifdef DISPLAY_STACK_FREE
            UBaseType_t uxHighWaterMark;
            char msg[300];
            char uptime_str[30];
            //char mqtt_topic[0].stats[50];
            
            status.uptime = esp_timer_get_time()/1000000;
            printUptime(status.uptime, uptime_str);
            
            /* Inspect our own high water mark on entering the task. */
            uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        
            MY_LOGI(TAG, "FREERTOS STACK: %d, Heap free size: %d",
            		uxHighWaterMark, xPortGetFreeHeapSize());

            sprintf(msg, "ESP Heap: %d, ESP min Heap: %d\nCPU usage: %d%%\nIP: %s\nuptime:%s\nTime: %s",
            		esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), status.cpu_usage, status.ip_addr, uptime_str,status.local_time);

            ESP_LOGI(TAG, "%s", msg);
            strcat(tasks_stats, msg);

            if(settings.connection_mode == WIFI_MODE)
            {
            	//status.wifi_rssi = get_wifi_rssi(); //get rssi in main loop
            	sprintf(msg, "\nWifi RSSI:%d", status.wifi_rssi);
            	strcat(tasks_stats, msg);
            }

            sprintf(msg, "Err flags:0x%08X", status.error_flags);
            ESP_LOGI(TAG, "%s", msg);
            strcat(tasks_stats, "\n");
            strcat(tasks_stats, msg);

            if ((settings.mqtt1_loglevel>=ESP_LOG_INFO) && !checkTagLoglevel(TAG))
            	mqtt_publish(0, mqtt_topic[0].publish, "log", tasks_stats, 0, settings.publish_server[0].mqtt_qos, 0);
            if ((settings.mqtt2_loglevel>=ESP_LOG_INFO) && !checkTagLoglevel(TAG))
            	mqtt_publish(1, mqtt_topic[1].publish, "log", tasks_stats, 0, settings.publish_server[1].mqtt_qos, 0);
#endif
            free(tasks_stats); 
        }
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
    }        
}

void start_control_task()
{ 
    xTaskCreate(control_task, "control_task", 1024*4, NULL, 12, NULL);
}
