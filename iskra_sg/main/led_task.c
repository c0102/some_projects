#include "main.h"

#ifdef LED_ENABLED
static const char *TAG = "LED";
#endif

//led definitions
#if CONFIG_BOARD_SG
static led_t led_array[]=
{
		{LED_PIN_RED, 0, 0},
		{LED_PIN_GREEN, 0, 0},
};
#endif

#if defined(CONFIG_BOARD_OLIMEX_ESP32_GATEWAY) || defined(CONFIG_BOARD_ESP32_DEMO)
static led_t led_array[]=
{
		{LED_PIN_RED, 0, 0}, //demo board has only one led
};
#endif

void led_set(int color, int mode)
{
#ifdef LED_ENABLED
	int i;

	//clear array
	for(i = 0; i < sizeof(led_array)/sizeof(led_t); i++)
	{
		led_array[i].mode = LED_OFF;
		led_array[i].state = LED_OFF;
	}

	//set mode
	if(color == LED_ORANGE)
	{
		led_array[LED_RED].mode = mode;
		led_array[LED_GREEN].mode = mode;
	}
	else
		led_array[color].mode = mode;

	//set state if mode is on or off
	if(mode == LED_ON || mode == LED_OFF)
	{
		if(color == LED_ORANGE)
		{
			led_array[LED_RED].state = mode;
			led_array[LED_GREEN].state = mode;
		}
		else
			led_array[color].state = mode;
	}

#if 0
	MY_LOGI(TAG, "LED states:");
	for(i = 0; i < sizeof(led_array)/sizeof(led_t); i++)
	{
		MY_LOGI(TAG, "LED%d mode:%d state:%d ", led_array[i].port, led_array[i].mode, led_array[i].state);
	}
#endif //if 0

#endif
}

#ifdef LED_ENABLED
static void toggle_leds(int blink_mode)
{
	int i;

	for(i = 0; i < sizeof(led_array)/sizeof(led_t); i++)
	{
		if(led_array[i].mode == blink_mode)
		{
			if(led_array[i].state == LED_OFF)
				led_array[i].state = LED_ON;
			else
				led_array[i].state = LED_OFF;
		}
	}
}

//toggle slow blinking leds
static void periodic_timer_slow_callback(void* arg)
{
	toggle_leds(LED_BLINK_SLOW);
}

//toggle fast blinking leds
static void periodic_timer_fast_callback(void* arg)
{
	toggle_leds(LED_BLINK_FAST);
}

static void led_timer_init()
{
	const esp_timer_create_args_t periodic_timer_slow_args = {
			.callback = &periodic_timer_slow_callback,
			/* name is optional, but may help identify the timer when debugging */
			.name = "blink slow"
	};

	const esp_timer_create_args_t periodic_timer_fast_args = {
			.callback = &periodic_timer_fast_callback,
			/* name is optional, but may help identify the timer when debugging */
			.name = "blink fast"
	};

	esp_timer_handle_t periodic_timer_slow;
	esp_timer_handle_t periodic_timer_fast;

	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_slow_args, &periodic_timer_slow));
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_fast_args, &periodic_timer_fast));
	/* The timer has been created but is not running yet */

	/* Start the timers */
	ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer_slow, LED_BLINK_SLOW_PERIOD));
	ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer_fast, LED_BLINK_FAST_PERIOD));
	MY_LOGI(TAG, "Started LED timers, time since boot: %lld us", esp_timer_get_time());
}

static void led_gpio_init()
{
    gpio_config_t io_conf;
    int i;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 0;
    for(i = 0; i < sizeof(led_array)/sizeof(led_t); i++)
    {
    	MY_LOGI(TAG, "LED GPIO init pin:%d ", led_array[i].port);
    	io_conf.pin_bit_mask |= (1ULL<<led_array[i].port);  //io_conf.pin_bit_mask = GPIO_LED_PIN_SEL;
    	led_array[i].mode = LED_OFF;
    	led_array[i].state = LED_OFF;
    }
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

void led_task(void *pvParam)
{
	int i;

	MY_LOGI(TAG, "Started");
	led_gpio_init();

	led_timer_init();

	while(1)
	{
		for(i = 0; i < sizeof(led_array)/sizeof(led_t); i++)
		{
			gpio_set_level(led_array[i].port, led_array[i].state);
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
#endif //LED_ENABLED
