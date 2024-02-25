/* Pulse counter module - Example
   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef ENABLE_PULSE_COUNTER
#include "driver/pcnt.h"

static const char *TAG = "DIG_INPUT";

/**
 *
 * Use PCNT module to count rising edges
 *
 * Functionality of GPIOs:
 *   - GPIO39 - pulse input pin
 *
 * An interrupt will be triggered when the counter value:
 *   - reaches 'thresh1' or 'thresh0' value,
 *   - reaches 'l_lim' value or 'h_lim' value,
 *   - will be reset to zero.
 */
#define PCNT_TEST_UNIT      PCNT_UNIT_0

//#define PCNT_H_LIM_VAL      10
//#define PCNT_L_LIM_VAL     -10
//#define PCNT_THRESH1_VAL    10
//#define PCNT_THRESH0_VAL   -5

#define PCNT_INPUT_SIG_IO   39  // Pulse Input GPIO

xQueueHandle pcnt_evt_queue;   // A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle

/* A sample structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct {
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

/* Decode what PCNT's unit originated an interrupt
 * and pass this information together with the event type
 * the main program using a queue.
 */
static void IRAM_ATTR pcnt_example_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    int i;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < PCNT_UNIT_MAX; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            /* Save the PCNT event type that caused an interrupt
               to pass it to the main program */
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);
            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static void pcnt_example_init(void)
{
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IO,
        //.ctrl_gpio_num = PCNT_INPUT_CTRL_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_TEST_UNIT,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_DIS,   // Keep the counter value on the positive edge
        .neg_mode = PCNT_COUNT_INC,   // Count up on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // Set the maximum and minimum limit values to watch
        //.counter_h_lim = PCNT_H_LIM_VAL,
        //.counter_l_lim = PCNT_L_LIM_VAL,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_TEST_UNIT, 1000);
    pcnt_filter_enable(PCNT_TEST_UNIT);

    /* Set threshold 0 and 1 values and enable events to watch */
#if 1
    pcnt_set_event_value(PCNT_TEST_UNIT, PCNT_EVT_THRES_1, settings.pcnt_threshold/*PCNT_THRESH1_VAL*/);
    pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
    //pcnt_set_event_value(PCNT_TEST_UNIT, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
    //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_0);
    /* Enable events on zero, maximum and minimum limit values */
    //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);
    pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);
    //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_L_LIM);
#endif

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_TEST_UNIT);
    pcnt_counter_clear(PCNT_TEST_UNIT);

    /* Register ISR handler and enable interrupts for PCNT unit */
    pcnt_isr_register(pcnt_example_intr_handler, NULL, 0, &user_isr_handle);
    pcnt_intr_enable(PCNT_TEST_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_TEST_UNIT);
}

void pulse_counter_task(void *pvParameters)
{
	int refresh_time = 1000; //1 second

    /* Initialize PCNT event queue and PCNT functions */
    pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
    pcnt_example_init();

    int16_t count = 0;
    int count_start = 0;
    pcnt_evt_t evt;
    portBASE_TYPE res;

#ifdef EEPROM_STORAGE
    count_start = ee_status.dig_in_count;
    if(count_start == 0xffffffff)
    	count_start = 0;
#endif

    MY_LOGI(TAG, "Start counter value :%d", count_start);

    while (1) {
    	/* Wait for the event information passed from PCNT's interrupt handler.
    	 * Once received, decode the event type and print it on the serial monitor.
    	 */
    	res = xQueueReceive(pcnt_evt_queue, &evt, refresh_time / portTICK_PERIOD_MS);
    	pcnt_get_counter_value(PCNT_TEST_UNIT, &count);//get counter after event or timeout
    	if (res == pdTRUE) {
    		printf("Event PCNT unit[%d]; cnt: %d\n", evt.unit, count);
    		status.dig_in_count = count_start + count;//todo count has only 16 bits
    		MY_LOGI(TAG, "PCNT counter value :%d", status.dig_in_count);
#ifdef DEMO_BOX
    			pcnt_counter_pause(PCNT_TEST_UNIT);
    			bicom_485_toggle(status.bicom_device[2].index);
/*
    			status.bicom_device[1].status = get_bicom_485_state(status.bicom_device[1].index);

    			if(status.bicom_device[1].status == 0)
    				set_bicom_485_device_state(status.bicom_device[1].index, 1);
    			else
    				set_bicom_485_device_state(status.bicom_device[1].index, 0);
*/
    			pcnt_counter_resume(PCNT_TEST_UNIT);
#endif
    		if (evt.status & PCNT_STATUS_THRES1_M) { //at threshold save value to eeprom and reset counter
    			//printf("THRES1 EVT\n");
    			MY_LOGI(TAG, "THRES1 EVT counter value :%d", status.dig_in_count);
    			pcnt_counter_pause(PCNT_TEST_UNIT);
    			count_start = status.dig_in_count; //save current value as start value
#ifdef EEPROM_STORAGE
    			ee_status.dig_in_count = status.dig_in_count;
    			EE_write_status();
#endif
    			pcnt_counter_clear(PCNT_TEST_UNIT);
    			pcnt_counter_resume(PCNT_TEST_UNIT);
    		}
#if 0
    		if (evt.status & PCNT_STATUS_THRES0_M) {
    			printf("THRES0 EVT\n");
    		}
    		if (evt.status & PCNT_STATUS_L_LIM_M) {
    			printf("L_LIM EVT\n");
    		}
    		if (evt.status & PCNT_STATUS_H_LIM_M) {
    			printf("H_LIM EVT\n");
    		}
    		if (evt.status & PCNT_STATUS_ZERO_M) {
    			printf("ZERO EVT\n");
    		}
#endif
    	}

    	//check if stop task has been requested
    	EventBits_t uxBits;
    	uxBits = xEventGroupWaitBits(ethernet_event_group, COUNT_TASK_STOP, false, true, 10 / portTICK_PERIOD_MS);
    	if( ( uxBits & COUNT_TASK_STOP ) == COUNT_TASK_STOP )
    	{
    		MY_LOGI(TAG, "STOP COUNTER TASK!");
#ifdef EEPROM_STORAGE
    		if(ee_status.dig_in_count != status.dig_in_count)//save counter if there is a change
    		{
    			ee_status.dig_in_count = status.dig_in_count;//update digital input counter
    			EE_write_status();
    		}
#endif
    		break; //exit task's main loop
    	}
    }//while 1

    if(user_isr_handle) {
        //Free the ISR service handle.
        esp_intr_free(user_isr_handle);
        user_isr_handle = NULL;
    }

    vTaskDelete(NULL);
}

void reset_pulse_counter()
{
	//if(settings.pulse_cnt_enabled)
	//{
		MY_LOGI(TAG, "Reset counter");
		pcnt_counter_pause(PCNT_TEST_UNIT);
		pcnt_counter_clear(PCNT_TEST_UNIT);
		status.dig_in_count = 0; //update status
#ifdef EEPROM_STORAGE
		ee_status.dig_in_count = 0; //save to eeprom
		EE_write_status();
#endif

		pcnt_counter_resume(PCNT_TEST_UNIT);
	//}
}
#endif //ENABLE_PULSE_COUNTER
