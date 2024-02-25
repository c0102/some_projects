#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/mcpwm.h"
#include "common.h"

#include "beacon.h"


static const char *TAG = "BEACON";

static app_settings_t* app_settings;
static outputs_state_t* out_state;

// You can get these value from the datasheet of servo you use, in general pulse width varies between 1000 to 2000 mocrosecond
#define BEACON_MIN_PULSEWIDTH_US (1000) // Minimum pulse width in microsecond
#define BEACON_MAX_PULSEWIDTH_US (2000) // Maximum pulse width in microsecond
#define BEACON_MAX_DEGREE        (90)   // Maximum angle in degree upto which servo can rotate

#define SERVO_PULSE_GPIO        (18)   // GPIO connects to the PWM signal line

static inline uint32_t convert_percentage_to_duty_us(int percentage)
{
    return (percentage *(BEACON_MAX_PULSEWIDTH_US - BEACON_MIN_PULSEWIDTH_US)/100) + BEACON_MIN_PULSEWIDTH_US;
}


static uint8_t state_changed = false;


esp_err_t beacon_OFF(void){

  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BEACON_OFF)
      {
        state_changed = true;
        out_state->beacon_state = BEACON_OFF;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BEACON_OFF] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t beacon_ON(void){
  
  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BEACON_ON)
      {
        state_changed = true;
        out_state->beacon_state = BEACON_ON;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BEACON_ON] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t beacon_SLOW(void){
  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BLINK_SLOW)
      {
        state_changed = true;
        out_state->beacon_state = BLINK_SLOW;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BEACON_SLOW] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t beacon_NORMAL(void){
  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BLINK_NORMAL)
      {
        state_changed = true;
        out_state->beacon_state = BLINK_NORMAL;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BLINK_NORMAL] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t beacon_FAST(void){
  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BLINK_FAST)
      {
        state_changed = true;
        out_state->beacon_state = BLINK_FAST;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BLINK_FAST] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t beacon_FAULT_1(void){
  if(xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(200))){
      if(out_state->beacon_state != BLINK_FAULT_1)
      {
        state_changed = true;
        out_state->beacon_state = BLINK_FAULT_1;
      }
      
      xSemaphoreGive(out_state->mutex);
  }else{
    ESP_LOGE(TAG,"[BLINK_FAULT_1] couldn't take out_state mutex!");
    return ESP_FAIL;
  }
  return ESP_OK;
}



void beacon_task(void* params){

    static uint64_t lastTime = 0;
    static uint8_t isOn = 0;
    while(1){

    if((xSemaphoreTake(out_state->mutex, pdMS_TO_TICKS(500)) == pdTRUE) &&
        xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(500) == pdTRUE)){

        switch(out_state->beacon_state){
            case BEACON_OFF:
              isOn = false;
              ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     BEACON_MIN_PULSEWIDTH_US));
              break;
            case BEACON_ON:
              isOn = true;
              ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     BEACON_MAX_PULSEWIDTH_US));
              break;

            case BLINK_SLOW:
              if(state_changed)
              {
                state_changed = false;
                isOn = true;
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 
                                                    convert_percentage_to_duty_us(app_settings->slow_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d%%", convert_percentage_to_duty_us(app_settings->slow_blink_intensity));
                lastTime = esp_timer_get_time();
              }else if(!isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->slow_blink_off_duration)){ 
                isOn = true;

                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     convert_percentage_to_duty_us(app_settings->slow_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d || ", convert_percentage_to_duty_us(app_settings->slow_blink_intensity));
                lastTime = esp_timer_get_time()/1000; //convert to ms 
              }else if(isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->slow_blink_on_duration)){
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     BEACON_MIN_PULSEWIDTH_US));
                ESP_LOGD(TAG, "Beacon should be off");
                isOn = false;
                lastTime = esp_timer_get_time()/1000; //convert to ms
              }
              break;
            case BLINK_NORMAL:
              if(state_changed)
              {
                state_changed = false;
                isOn = true;  
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 
                                                    convert_percentage_to_duty_us(app_settings->normal_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d", convert_percentage_to_duty_us(app_settings->normal_blink_intensity));
                lastTime = esp_timer_get_time()/1000; //convert to ms
              }else if(!isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->normal_blink_off_duration)){ 
                isOn = true;
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     convert_percentage_to_duty_us(app_settings->normal_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d", convert_percentage_to_duty_us(app_settings->normal_blink_intensity));
                lastTime = esp_timer_get_time()/1000; //convert to ms
              }else if(isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->normal_blink_on_duration)){
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     BEACON_MIN_PULSEWIDTH_US));
                ESP_LOGD(TAG, "Beacon should be off");
                isOn = false;
                lastTime = esp_timer_get_time()/1000; //convert to ms
              }
              break;
            case BLINK_FAST:
              if(state_changed)
              {
                state_changed = false;
                isOn = true;
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 
                                                    convert_percentage_to_duty_us(app_settings->fast_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d", convert_percentage_to_duty_us(app_settings->fast_blink_intensity));
                lastTime = esp_timer_get_time()/1000; //convert to ms
              }else if(!isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->fast_blink_off_duration)){ 
                isOn = true;  
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     convert_percentage_to_duty_us(app_settings->fast_blink_intensity)));
                ESP_LOGD(TAG, "Beacon is on with pulse width %d", convert_percentage_to_duty_us(app_settings->fast_blink_intensity));
                lastTime = esp_timer_get_time();  
              }else if(isOn && (((esp_timer_get_time()/1000) - lastTime) > app_settings->fast_blink_on_duration)){
                ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,  
                                                     BEACON_MIN_PULSEWIDTH_US));
                ESP_LOGD(TAG, "Beacon should be off");
                isOn = false;
                lastTime = esp_timer_get_time();
              }
              break;
            case BLINK_FAULT_1:
              break;
            case BLINK_FAULT_2:
              break; 
            default:
                break;
        }//switch

         xSemaphoreGive(out_state->mutex);
         xSemaphoreGive(app_settings->mutex);

    }//if

    vTaskDelay(pdMS_TO_TICKS(500));
  
  }// while(1)
}// beacon_task()

esp_err_t beacon_init(app_settings_t* settings, outputs_state_t* state)
{
    app_settings = settings;
    out_state = state;

    if(xSemaphoreTake(app_settings->mutex, pdMS_TO_TICKS(250)) == pdTRUE){
        mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, app_settings->beacon_pin); // To drive a RC servo, one MCPWM generator is enough

        mcpwm_config_t pwm_config = {
            .frequency = 50, // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
            .cmpr_a = 0,     // duty cycle of PWMxA = 0
            .counter_mode = MCPWM_UP_COUNTER,
            .duty_mode = MCPWM_DUTY_MODE_0,
        };
        mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    xSemaphoreGive(app_settings->mutex);
    }else{
        ESP_LOGE(TAG, "app_settings mutex take failed!");
        return ESP_FAIL; 
    }

    xTaskCreatePinnedToCore(beacon_task, "beacon_task", 1024*3, NULL, 8, NULL, 1);

    return ESP_OK;

    // while (1) {
    //     for (int angle = -SERVO_MAX_DEGREE; angle < SERVO_MAX_DEGREE; angle++) {
    //         ESP_LOGI(TAG, "Angle of rotation: %d", angle);
    //         ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, example_convert_servo_angle_to_duty_us(angle)));
    //         vTaskDelay(pdMS_TO_TICKS(100)); //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation under 5V power supply
    //     }
    // }
}
