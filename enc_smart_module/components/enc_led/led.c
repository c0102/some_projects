/*
 * Created on Thu Jun 09 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#include "led.h"
#include "led_strip.h"


static const int LED = 8;
static led_strip_t strip = {
        .type = LED_STRIP_SK6812,
        .length = 1,
        .gpio = LED,
        .buf = NULL,
        .is_rgbw = true,
#ifdef LED_STRIP_BRIGHTNESS
        .brightness = 255,
#endif
    };

void init_led(int led)
{
  led_strip_install();
  strip.gpio = led;  
  ESP_ERROR_CHECK(led_strip_init(&strip));

} 

void toggle_led_red(bool is_on)
{
  rgb_t color ={.r = is_on ? 255:0, .b = 0, .g = 0}; 
  strip.brightness = 50;
  led_strip_set_pixel( &strip, 0, color);

  led_strip_flush(&strip);
}
