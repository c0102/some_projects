/*
 * graph.c
 *
 *  Created on: Dec 9, 2019
 *      Author: iskra
 */

#include "main.h"

#define NUM_BARS (24*4)
#define BAR_WIDTH 8
#define GRAPH_WIDTH (NUM_BARS*BAR_WIDTH)
#define GRAPH_HIGHT (GRAPH_WIDTH/3)
#define GRAPH_X_START 65
#define GRAPH_Y_START 10
#define MAX_INPUT_VAL (250*80*3)
#define LEGEND_X_START (GRAPH_X_START + 250)
#define LEGEND_Y_START (GRAPH_Y_START + GRAPH_HIGHT + 50)
#define RUSSIAN 1

extern float power_buf[100];

static const char *TAG = "graph";

esp_err_t drawGraph_handler(httpd_req_t *req)
{
	int power_max_value;
	int power_min_value;
	float total_power;
	char temp[150];
	int i;
	int power_samples;
	int language = 0;
	int uart_port = -1;
	int modbus_address;

	httpd_resp_set_type(req, "application/json");

	//HTTP.send ( 200, "image/svg+xml", out);
	httpd_resp_set_type(req, "image/svg+xml");

	sprintf(temp, "<svg xmlns='http://www.w3.org/2000/svg' version='1.1' width='%d' height='%d'>\n", GRAPH_WIDTH + 100, GRAPH_HIGHT + 200);
	httpd_resp_sendstr_chunk(req, temp);

	sprintf(temp, "<rect x='%d' y='%d' width='%d' height='%d' fill='rgb(242, 242, 242)' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HIGHT);
	httpd_resp_sendstr_chunk(req, temp);

	httpd_resp_sendstr_chunk(req, "<g stroke=\"black\">\n");

	if (strlen(req->uri) > 11 && strncmp(req->uri, "/graph/", 7) == 0)
	{
		int number = -1;
		if(strstr(req->uri, ".svg") != NULL)
		{
			if(isdigit(req->uri[7]))
				number = req->uri[7] - '0';
			if(isdigit(req->uri[8]))
				number = number*10 + req->uri[8] -'0';
			if(number >= 0 && number < 18)
			{
				uart_port = status.modbus_device[number - 1].uart_port;
				modbus_address = status.modbus_device[number - 1].address;
			}
		}
	}

	if(uart_port < 0)//error
	{
		MY_LOGW(TAG, "No active device for modbus readings. Uri:%s", req->uri);
		//httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No active port");
		sprintf(temp, "<text x='%d' y='%d'>Device not available or configuration error</text>\n", GRAPH_X_START + 200, GRAPH_HIGHT / 2);
		httpd_resp_sendstr_chunk(req, temp);

		httpd_resp_sendstr_chunk(req, "</g>\n</svg>\n");

		/* Send empty chunk to signal HTTP response completion */
		httpd_resp_sendstr_chunk(req, NULL);
		return ESP_FAIL;
	}

	power_samples = getRamLoggerData(uart_port, modbus_address);
	if (power_samples < 1)
	{
		MY_LOGE(TAG,"Not enough samples: %d", power_samples);
		statistics.errors++;

		if (language == RUSSIAN) //russian
			sprintf(temp, "<text x='%d' y='%d'>Недостаточно данных. Подождите 15 минут после включения.</text>\n", GRAPH_X_START + 200, GRAPH_HIGHT / 2 + 10); //y text
		else
			sprintf(temp, "<text x='%d' y='%d'>Not enough data (%d). Wait for 15 minutes after powerup and check connection mode</text>\n", GRAPH_X_START + 100, GRAPH_HIGHT / 2 + 10, power_samples); //y text

		httpd_resp_sendstr_chunk(req, temp);

		httpd_resp_sendstr_chunk(req, "</g>\n</svg>\n");

		/* Send empty chunk to signal HTTP response completion */
		httpd_resp_sendstr_chunk(req, NULL);
		return ESP_FAIL;
	}

	total_power = 0;
	power_max_value = -0x7fffffff;
	power_min_value = 0x7fffffff; //max value for start

	for (i = 0; i < power_samples; i++)
	{
		//int tmp_power = power_buf[i];
		//tmp_power /= 4;   //to kWh because we use 15 minute intervals
		total_power += power_buf[i];;
		if (power_buf[i] > power_max_value) //save max value
			power_max_value = power_buf[i];
		if (power_buf[i] < power_min_value) //save min value
			power_min_value = power_buf[i];
	}

	total_power /= 4; //to kWh because we use 15 minute intervals
	MY_LOGI(TAG, "total_power: %f Wh", total_power);

	//calculate height of graph
	int graph_max_value = 0;
	int graph_min_value = 0;

	int graph_height;
	if(power_min_value > 0)
		graph_height = power_max_value; //complete positive graph
	else if(power_max_value < 0)
		graph_height = 0 - power_min_value; //complete negative height
	else
		graph_height = power_max_value - power_min_value; //complete height

	//round up min, max
	if (graph_height < 10 && graph_height >=0)//less than 10W
	{
		graph_max_value = 10;
		graph_min_value = 0;
	}
	else //round up
	{
		int j;
		for(j = 1; j < graph_height; j*=10)
		{
			if(power_max_value > 0)
				graph_max_value = ((power_max_value / j) + 1) * j;
			if(power_min_value < 0)
				graph_min_value = ((power_min_value / j) - 1) * j;
		}
	}

	if(graph_max_value < power_max_value)
		graph_max_value*= 2;

	if(graph_min_value > power_min_value)
		graph_min_value*= 2;

	//update height to rounded min max
	graph_height = graph_max_value - graph_min_value; //rounded height
	if(graph_height == 0)
		graph_height = 1; //prevent div 0 //return ESP_FAIL;

	int graph_zero = graph_max_value * GRAPH_HIGHT / graph_height + GRAPH_Y_START;

	MY_LOGI(TAG,"Power max      : %d", power_max_value);
	MY_LOGI(TAG,"Power min      : %d", power_min_value);
	MY_LOGI(TAG,"Graph max scale: %d", graph_max_value);
	MY_LOGI(TAG,"Graph min scale: %d", graph_min_value);
	MY_LOGI(TAG,"Graph height   : %d", graph_height);
	MY_LOGI(TAG,"Graph zero     : %d", graph_zero);
	MY_LOGI(TAG,"No of samples  : %d", power_samples);
	MY_LOGI(TAG,"GRAPH_HIGHT    : %d", GRAPH_HIGHT);
	MY_LOGI(TAG,"GRAPH_Y_START  : %d",	GRAPH_Y_START);

	//trenutni bar je: ure*4 + minute/4
	time_t now;
	time(&now);
	struct tm * timeinfo = localtime(&now);

	int start_bar = timeinfo->tm_hour * 4 + (timeinfo->tm_min) / 15 - 1;
	//debug_print(F("Start bar:"));
	//debug_println(start_bar);

#define SVG_BAR_CHART
#ifdef SVG_BAR_CHART

	char *color;
	char *color1 = {"rgb(77,166,255)"}; //blue for today
	char *color2 = {"rgb(153,153,153)"}; //gray for yesterday
	color = color1;
	int x, y;

	for (i = 0; i < power_samples; i++)
	{
		int curr_bar = start_bar - i; //from right to left
		if (curr_bar < 0) //yesterday
		{
			curr_bar += NUM_BARS;
			color = color2; //different color for yesterday events
		}
		//DEBUG_UART.printf("BAR:%d val:%d\n\r",curr_bar, power_buf[i]);

		y = ((power_buf[i] * GRAPH_HIGHT) / graph_height); //int y = rand() % GRAPH_HIGHT - 20;
		//MY_LOGI(TAG,"y[%d]   : %d", i, y);

		x = GRAPH_X_START + curr_bar * BAR_WIDTH;

		if(y >= 0)
			sprintf(temp, "<rect x='%d' y='%d' fill='%s' stroke='rgb(242, 242, 242)' width='%d' height='%d'/>\n", x, graph_zero - y, color, BAR_WIDTH - 1, y);
		else
			sprintf(temp, "<rect x='%d' y='%d' fill='%s' stroke='rgb(242, 242, 242)' width='%d' height='%d'/>\n", x, graph_zero, color, BAR_WIDTH - 1, abs(y));

		//MY_LOGI(TAG,"%s", temp);
		httpd_resp_sendstr_chunk(req, temp);
	}
#else //LINE CHART
	if (power_samples > 1 && graph_height)
	{
		int rollover = 0;
		int graph_zero = graph_min_value * GRAPH_HIGHT / graph_height;
		//debug_print(F("Graph_zero px: "));
//		debug_println(graph_zero);
//		debug_print(F("Graph_max px: "));
//		debug_println(GRAPH_HIGHT);
//		debug_print(F("Graph_min px: "));
//		debug_println(GRAPH_Y_START);

		//draw todays time line
		sprintf(temp, "<rect x='%d' y='%d' width='%d' height='%d' fill='rgb(128, 204, 255)' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", GRAPH_X_START, GRAPH_Y_START, (start_bar + 1) * BAR_WIDTH, GRAPH_HIGHT);
		httpd_resp_sendstr_chunk(req, temp);
		//sprintf(temp, "<polyline points=\"20,20 40,25 60,40 80,120 120,140 200,180\" ");
		sprintf(temp, "<polyline points=\"");
		httpd_resp_sendstr_chunk(req, temp);
		for (i = 0; i < power_samples; i++)
		{
			int curr_bar = start_bar - i; //from right to left
			if (curr_bar < 0) //yesterday
			{
				curr_bar += NUM_BARS;  //move to right
				if (rollover == 0)
				{
					rollover = 1;
					httpd_resp_sendstr_chunk(req, "\" "); //end of points
					sprintf(temp, "style=\"fill:none;stroke:black;stroke-width:3\"/>");
					httpd_resp_sendstr_chunk(req, temp);
					sprintf(temp, "<polyline points=\""); //start new line
					httpd_resp_sendstr_chunk(req, temp);
				}
			}
			if (power_buf[i] >= 0)
				sprintf(temp, "%d,%d", GRAPH_X_START + (curr_bar + 1) * BAR_WIDTH, GRAPH_HIGHT + GRAPH_Y_START - ((power_buf[i] * GRAPH_HIGHT) / graph_height) + graph_zero);
			else
				sprintf(temp, "%d,%d", GRAPH_X_START + (curr_bar + 1) * BAR_WIDTH, GRAPH_HIGHT + GRAPH_Y_START - ((((graph_min_value - power_buf[i]) * graph_zero) / abs(graph_min_value))));
			httpd_resp_sendstr_chunk(req, temp);
			if (i != power_samples - 1)
				httpd_resp_sendstr_chunk(req, ",");
			else
				httpd_resp_sendstr_chunk(req, "\" "); //end of points
		}
		sprintf(temp, "style=\"fill:none;stroke:black;stroke-width:3\"/>");
		httpd_resp_sendstr_chunk(req, temp);
	}
#endif

	//vodoravne crte
	char strval[20];
	int divider = 1000; //scale in kWatts
	char unit[3];

	if (graph_height < 1000) //scale in Watts
	{
		strcpy(unit, "W");
		divider = 1;
	}
	else
		strcpy(unit, "kW");

	for (i = 0; i < 4; i++)
	{
		int precision = 0;

		sprintf(temp, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke-width='1' stroke='rgb(217, 217, 217)' />\n", GRAPH_X_START - 5 , (4 - i) * (GRAPH_HIGHT / 4) + GRAPH_Y_START, GRAPH_WIDTH + GRAPH_X_START, (4 - i) * (GRAPH_HIGHT / 4) + GRAPH_Y_START);
		httpd_resp_sendstr_chunk(req, temp);

		if (graph_height * i % (4 * divider))
			precision = 2;
		//dtostrf(((graph_max_value * i + (graph_min_value * (4 - i))) / (float)(4 * divider)), 4, precision, strval);
		sprintf(strval, "%.*f", precision, ((graph_max_value * i + (graph_min_value * (4 - i))) / (float)(4 * divider)));
		sprintf(temp, "<text x='2' y='%d'>%s %s</text>\n", (4 - i) * (GRAPH_HIGHT / 4) + 5 + GRAPH_Y_START, strval, unit); //x text
		httpd_resp_sendstr_chunk(req, temp);
	}
	//dtostrf(graph_max_value / (float)divider, 4, 0, strval);
	sprintf(strval, "%.*f", 0, graph_max_value / (float)divider);
	sprintf(temp, "<text x='2' y='%d'>%s %s</text>\n", 0 + 5 + GRAPH_Y_START, strval, unit); //x max value
	httpd_resp_sendstr_chunk(req, temp);

	//x os text
	for (i = 0; i < GRAPH_WIDTH + 10; i += GRAPH_WIDTH / 24)
	{
		//sprintf(temp, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", i + GRAPH_X_START , GRAPH_HIGHT + GRAPH_Y_START - 5, i + GRAPH_X_START, GRAPH_HIGHT + GRAPH_Y_START + 5);
		sprintf(temp, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", i + GRAPH_X_START , graph_zero - 5, i + GRAPH_X_START, graph_zero + 5);
		httpd_resp_sendstr_chunk(req, temp);
		if(i%64 == 0)
		{
			sprintf(temp, "<text x='%d' y='%d'>%d:00</text>\n", i < (5 * GRAPH_WIDTH / 12) ? i + GRAPH_X_START - 11 : i + GRAPH_X_START - 18, graph_zero + 20, i / 32); //y text
			httpd_resp_sendstr_chunk(req, temp);
		}
	}

	//legend
	sprintf(temp, "<rect x='%d' y='%d' width='%d' height='%d' fill='rgb(128, 204, 255)' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", LEGEND_X_START, LEGEND_Y_START, 50, 20);
	httpd_resp_sendstr_chunk(req, temp);
	if (language == RUSSIAN) //russian
		sprintf(temp, "<text x='%d' y='%d'>Сегодня</text>\n", LEGEND_X_START + 60, LEGEND_Y_START + 15);
	else //english
		sprintf(temp, "<text x='%d' y='%d'>Today</text>\n", LEGEND_X_START + 60, LEGEND_Y_START + 15);
	httpd_resp_sendstr_chunk(req, temp);
	sprintf(temp, "<rect x='%d' y='%d' width='%d' height='%d' fill='rgb(179, 179, 179)' stroke-width='1' stroke='rgb(0, 0, 0)' />\n", LEGEND_X_START + 140, LEGEND_Y_START, 50, 20);
	httpd_resp_sendstr_chunk(req, temp);
	if (language == RUSSIAN) //russian
		sprintf(temp, "<text x='%d' y='%d'>Вчера</text>\n", LEGEND_X_START + 200, LEGEND_Y_START + 15);
	else
		sprintf(temp, "<text x='%d' y='%d'>Yesterday</text>\n", LEGEND_X_START + 200, LEGEND_Y_START + 15);
	httpd_resp_sendstr_chunk(req, temp);

	if(total_power < 1000)
		sprintf(strval, "%.*f Wh", 2, total_power);
	else
		sprintf(strval, "%.*f kWh", 2, total_power / (float)1000);

	if (language == RUSSIAN) //russian
		sprintf(temp, "<text x='%d' y='%d'>Потребление энергии за последние 24 часа: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 55, strval);
	else
		sprintf(temp, "<text x='%d' y='%d'>Total energy in last 24 hours: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 55, strval);
	httpd_resp_sendstr_chunk(req, temp);

	if(1)//if(power_max_value)
	{
		if(power_max_value < 1000)
			sprintf(strval, "%d W", power_max_value);
		else
			sprintf(strval, "%.*f kW", 2, power_max_value / (float)1000);
		if (language == RUSSIAN) //russian todo:prevedi tole
			sprintf(temp, "<text x='%d' y='%d'>Потребление энергии за последние 24 часа: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 75, strval);
		else
		{
			sprintf(temp, "<text x='%d' y='%d'>Max power in last 24 hours: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 75, strval);
		}
		httpd_resp_sendstr_chunk(req, temp);
	}

	if(1)//if(power_min_value)
	{
		if(power_min_value > -1000 && power_min_value < 1000)
			sprintf(strval, "%d W", power_min_value);
		else
			sprintf(strval, "%.*f kW", 2, power_min_value / (float)1000);

		if (language == RUSSIAN) //russian todo:prevedi tole
			sprintf(temp, "<text x='%d' y='%d'>Потребление энергии за последние 24 часа: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 95, strval);
		else
		{
			sprintf(temp, "<text x='%d' y='%d'>Min power in last 24 hours: %s</text>\n", LEGEND_X_START, LEGEND_Y_START + 95, strval);
		}
		httpd_resp_sendstr_chunk(req, temp);
	}

	//#define DEBUG_GRAPH
#ifdef DEBUG_GRAPH

	sprintf(temp, "<text x='%d' y='%d'>Pmax:%d Pmin:%d Graph Max:%d Graph Min:%d Graph Height:%d Unit:%s Power samples:%d</text>\n", LEGEND_X_START - 225, LEGEND_Y_START + 115, power_max_value, power_min_value, graph_max_value, graph_min_value, graph_height, unit, power_samples);
	httpd_resp_sendstr_chunk(req, temp);

	char temp1[200];
	char temp2[200];

	debug_print("Samples:");
	for(i = 0; i < power_samples/4; i++)
	{
		sprintf(temp, "%d,", power_buf[i]);
		debug_print(temp);
		strcat(temp1, temp);
	}

	sprintf(temp2, "<text x='%d' y='%d'>%s</text>\n", LEGEND_X_START - 250, LEGEND_Y_START + 135, temp1);
	httpd_resp_sendstr_chunk(req, temp2);

	strcpy(temp1, "");
	for(i = power_samples/4; i < (power_samples*2)/4; i++)
	{
		sprintf(temp, "%d,", power_buf[i]);
		debug_print(temp);
		strcat(temp1, temp);
	}

	sprintf(temp2, "<text x='%d' y='%d'>%s</text>\n", LEGEND_X_START - 250, LEGEND_Y_START + 155, temp1);
	httpd_resp_sendstr_chunk(req, temp2);

	strcpy(temp1, "");
	for(i = (power_samples*2)/4; i < (power_samples*3)/4; i++)
	{
		sprintf(temp, "%d,", power_buf[i]);
		debug_print(temp);
		strcat(temp1, temp);
	}

	sprintf(temp2, "<text x='%d' y='%d'>%s</text>\n", LEGEND_X_START - 250, LEGEND_Y_START + 175, temp1);
	httpd_resp_sendstr_chunk(req, temp2);

	strcpy(temp1, "");
	for(i = (power_samples*3)/4; i < power_samples; i++)
	{
		sprintf(temp, "%d,", power_buf[i]);
		debug_print(temp);
		strcat(temp1, temp);
	}

	sprintf(temp2, "<text x='%d' y='%d'>%s</text>\n", LEGEND_X_START - 250, LEGEND_Y_START + 140, temp1);
	httpd_resp_sendstr_chunk(req, temp2);

#endif

	char model_type[20];
	uint8_t serial_number[10];
	getModelAndSerial(uart_port, modbus_address, model_type, serial_number); //use measurement_data for serial number buffer

	MY_LOGD(TAG, "Model:%s serial:%s", model_type, serial_number);
	sprintf(temp, "<text x='%d' y='%d'>Device type: %s, serial number: %s</text>\n", LEGEND_X_START - 20, LEGEND_Y_START + 115, model_type, serial_number);
	httpd_resp_sendstr_chunk(req, temp);

	httpd_resp_sendstr_chunk(req, "</g>\n</svg>\n");

	//DEBUG_UART.printf("Graph len:%d heap:%d\n\r", out.length(), system_get_free_heap_size());
	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}
