/* ADC1 Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "main.h"
#ifdef ENABLE_PT1000

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/adc_channel.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#ifdef DEMO_BOX
#define NO_OF_SAMPLES   100          //for faster response
#else
#define NO_OF_SAMPLES   500          //Multisampling
#endif

static const char *TAG = "ADC";

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel_in8 =  ADC1_GPIO34_CHANNEL ;    //GPIO34
static const adc_channel_t channel_in15 = ADC1_GPIO35_CHANNEL ;    //GPIO35
static const adc_channel_t channel_in14 = ADC1_GPIO36_CHANNEL ;    //GPIO36

/* - 0dB attenuaton (ADC_ATTEN_DB_0) between 100 and 950mV
 * - 2.5dB attenuation (ADC_ATTEN_DB_2_5) between 100 and 1250mV
 * - 6dB attenuation (ADC_ATTEN_DB_6) between 150 to 1750mV
 * - 11dB attenuation (ADC_ATTEN_DB_11) between 150 to 2450mV */
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

int PT100R2T(long r);

static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        MY_LOGI(TAG,"eFuse Two Point: Supported");
    } else {
        MY_LOGI(TAG,"eFuse Two Point: NOT supported");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        MY_LOGI(TAG,"eFuse Vref: Supported");
    } else {
        MY_LOGI(TAG,"eFuse Vref: NOT supported");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        MY_LOGI(TAG,"Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        MY_LOGI(TAG,"Characterized using eFuse Vref");
    } else {
        MY_LOGI(TAG,"Characterized using Default Vref");
    }
}

#ifdef DEMO_BOX
void demo_box_task(void *pvParameters)
{
	MY_LOGI(TAG, "DEMO_BOX Task STARTED");

	while(1)
	{
		EventBits_t uxBits;

		uxBits = xEventGroupWaitBits(ethernet_event_group, ADC_TASK_STOP, false, true, 10 / portTICK_PERIOD_MS);
		if( ( uxBits & ADC_TASK_STOP ) == ADC_TASK_STOP )
		{
			MY_LOGI(TAG, "STOP ADC POLLING!");
			vTaskDelete( NULL );
		}

		status.bicom_device[1].status = get_bicom_485_state(status.bicom_device[1].index);
		if(status.bicom_device[1].status < 0 || status.bicom_device[1].status > 1) //no response
			vTaskDelay(pdMS_TO_TICKS(5000));//wait 5 seconds

		if(status.pt1000_temp > settings.temp_max_limit && status.bicom_device[1].status == 1)
		{
			MY_LOGI(TAG, "TEMP:%d > limit:%d", status.pt1000_temp, settings.temp_max_limit);
			set_bicom_485_device_state(status.bicom_device[1].index, 0);
			vTaskDelay(pdMS_TO_TICKS(300));
		}
		else if(status.pt1000_temp < settings.temp_min_limit && status.bicom_device[1].status == 0)
		{
			MY_LOGI(TAG, "TEMP:%d < limit:%d", status.pt1000_temp, settings.temp_min_limit);
			set_bicom_485_device_state(status.bicom_device[1].index, 1);
			vTaskDelay(pdMS_TO_TICKS(300));
		}

		vTaskDelay(pdMS_TO_TICKS(100));
	}

	vTaskDelete(NULL);
}
#endif

void adc_task(void *pvParameters)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(channel_in8, atten);
    adc1_config_channel_atten(channel_in14, atten);
    adc1_config_channel_atten(channel_in15, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

#ifdef DEMO_BOX
    xTaskCreate(&demo_box_task, "demo_box", 4096, NULL, 10, NULL);
#endif

    //Continuously sample ADC1
    while (1) {
        uint32_t adc_reading_in8 = 0;
        uint32_t adc_reading_in14 = 0;
        uint32_t adc_reading_in15 = 0;

        EventBits_t uxBits;

        uxBits = xEventGroupWaitBits(ethernet_event_group, ADC_TASK_STOP, false, true, 10 / portTICK_PERIOD_MS);
        if( ( uxBits & ADC_TASK_STOP ) == ADC_TASK_STOP )
        {
        	MY_LOGI(TAG, "STOP ADC POLLING!");
        	vTaskDelete( NULL );
        }

        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
        	adc_reading_in8 += adc1_get_raw((adc1_channel_t)channel_in8);
        	adc_reading_in14 += adc1_get_raw((adc1_channel_t)channel_in14);
        	adc_reading_in15 += adc1_get_raw((adc1_channel_t)channel_in15);
        	vTaskDelay(pdMS_TO_TICKS(10));
        }

        adc_reading_in8 /= NO_OF_SAMPLES;
        adc_reading_in14 /= NO_OF_SAMPLES;
        adc_reading_in15 /= NO_OF_SAMPLES;

        //Convert adc_reading to voltage in mV
        uint32_t voltage_in8 = esp_adc_cal_raw_to_voltage(adc_reading_in8, adc_chars);
        uint32_t voltage_in14 = esp_adc_cal_raw_to_voltage(adc_reading_in14, adc_chars);
        uint32_t voltage_in15 = esp_adc_cal_raw_to_voltage(adc_reading_in15, adc_chars);

        uint32_t URref = adc_reading_in8 - adc_reading_in15;
        uint32_t URx = adc_reading_in15 - adc_reading_in14;
        uint32_t URn = adc_reading_in14;

        if (URref<1)        // prekinjena veriga uporov
        {
        	MY_LOGI(TAG, "URef: %d", URref);
        	status.pt1000_temp = -5000; //invalid value
        }
        else
        {

#define ResRef 10000
        	int32_t ResX = ResRef*URx/URref;           // upornost Rx v Ohm/10
        	int32_t ResN = ResRef*URn/URref;           // upornost Rn v Ohm/10
        	int32_t Temp1000 = PT100R2T(ResX);

        	MY_LOGI(TAG,"IN8  Raw: %d\tVoltage: %dmV", adc_reading_in8, voltage_in8);
        	MY_LOGI(TAG,"IN15 Raw: %d\tVoltage: %dmV", adc_reading_in15, voltage_in15);
        	MY_LOGI(TAG,"IN14 Raw: %d\tVoltage: %dmV", adc_reading_in14, voltage_in14);
        	MY_LOGI(TAG, "Samples:%d URef:%d URx:%d Rx:%d.%d Rn:%d PT:%d.%d", NO_OF_SAMPLES, URref, URx, ResX/10, ResX%10, ResN, Temp1000/10, abs(Temp1000%10));

        	//vTaskDelay(pdMS_TO_TICKS(1000));
        	status.pt1000_temp = Temp1000;
        }
    }

    vTaskDelete(NULL);
}

#define PTR0 10000

const float PTK[5]={
 -(24575 / 5)     , //  '; K0 * -20
 154525 / 5       , //  '; K1 * 20
 43545 / 5 - 2    , //  '; K2 * 20
 -(10160 / 5 + 2) , //  '; K3 * -20
 33885 / 5 + 3    };//  '; K4 * 20
//'; * ZA NEGATIVNE TEMPERATURE:
const float PTKN[4]={
 -(24195 / 5)     , // '; KN0 * -20
 (145710 / 5 - 1) , // '; KN1 * 20
 109855 / 5       , // '; KN2 * 20
 -(167720 / 5)    };// '; KN3 * -20

int PT100R2T(long r)
{
	float t;
	int i;

    if (r < 0) return -2000;
    t = 0;
    if (r < PTR0) // Then    ' polinom ZA NEGATIVNE TEMPERATURE
    {
        for(i = 3; i >= 0 ;i--)
        {
            t = (t * r)/65536;
            t = t + PTKN[i];
        }
    }
    else   //             ' polinom ZA POZITIVNE TEMPERATURE
    {
        if (r > 4L * PTR0) return 8500;  // ' ;ZGORNJA MEJA
        for(i = 4; i >= 0 ;i--)
        {
            t = (t * r)/65536;
            t = t + PTK[i];
        }
    }
    i=(long)t;
    if (i>=0)
    	i++; 	// zaokrozevanje
    i>>=1;
    if (i>8500)
    	return 8500;
    if (i<-2000)
    	return -2000;
    return i;
}
#endif //ENABLE_PT1000
